/*
 * File:     main.c
 * Author:   Eric Wang
 * Date:     May 25, 2024
 * Description: Entry point for this simulation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include "lcgrand.h"
#include "datadef.h"


int read_config(void);
double expon(double);
void depart(void);
void arrive(void);
void time_tick(void);
void init(void);
bool double_equal(double, double);
void print_sim_data(void);
void packet_left_in_system(void);

Server _server_;
Queue* _queue_ = NULL;
Event _next_event_;
BitEmitter* _em_ = NULL;
Sim _sim_;
QUEUE_MODE _mode_;
snum_t _output_ctrl_;

// Source number in each category
int _source_num_[ALLTYPE] = {18, 15, 15};

// Only for WFQ mode
float _set_weight_[ALLTYPE] = {0.5, 0.3, 0.2};

// Main Entry
int main(int argc, char const *argv[]) {
    // Assign a initial value for global variables
    init();
    
    // Read configuration file
    read_config();
    
    // Start the simulation
    while (_sim_.sim_time < _sim_.max_sim_time)
    {
        if (_output_ctrl_ > _sim_.output_interval) {
            packet_left_in_system();
            print_sim_data();
            _output_ctrl_ = 0;
        }
        else {
            _output_ctrl_ += 1;
        }
        time_tick();
        _sim_.sim_time = _next_event_.event_time;
        switch (_next_event_.type) {
            case ARRIVAL:
                arrive();
                break;
            case DEPART:
                depart();
                break;
            default:
                continue;
        }
    }
    
    return 0;
}

void print_sim_data(void) {
    
    printf("-----------------------Sim time: %.4f------------------------------\n", _sim_.sim_time);
    printf("Packet served (audio, video, data): %ld, %ld, %ld \n",
           _sim_.class_served[AUDIO],
           _sim_.class_served[VIDEO],
           _sim_.class_served[DATA]);
    printf("Packet arrived (audio, video, data): %ld, %ld, %ld \n",
           _sim_.class_arrived[AUDIO],
           _sim_.class_arrived[VIDEO],
           _sim_.class_arrived[DATA]);
    
    printf("Packet delayed (audio, video, data): %ld, %ld, %ld \n",
           _sim_.class_delayed[AUDIO],
           _sim_.class_delayed[VIDEO],
           _sim_.class_delayed[DATA]);
    
    
    printf("Package dropped (audio, video, data): %ld, %ld, %ld \n",
           _sim_.class_dropped[AUDIO],
           _sim_.class_dropped[VIDEO],
           _sim_.class_dropped[DATA]);

    printf("Packet served in kb (audio, video, data): %0.4Lf, %0.4Lf, %0.4Lf \n",
           _sim_.class_served_kb[AUDIO],
           _sim_.class_served_kb[VIDEO],
           _sim_.class_served_kb[DATA]);
    
    printf("Packet remained (audio, video, data): %ld, %ld, %ld \n",
           _sim_.class_remained[AUDIO],
           _sim_.class_remained[VIDEO],
           _sim_.class_remained[DATA]);
    
    printf("Total served packet in kb: %0.4Lf \n", _sim_.total_served_kb);
    
    lfnum_t totoal_served_kb = _sim_.total_served_kb <= 0 ? 10 : _sim_.total_served_kb;
    
    printf("Percentage of Served (audio, video, data): %0.4Lf, %0.4Lf, %0.4Lf \n",
           _sim_.class_served_kb[AUDIO] / totoal_served_kb,
           _sim_.class_served_kb[VIDEO] / totoal_served_kb,
           _sim_.class_served_kb[DATA] / totoal_served_kb);
}

void packet_left_in_system(void)
{
    int i;
    for (i = 0; i < ALLTYPE; ++i) {
        _sim_.class_remained[i] = 0;
    }
    
    // Packets remained in system
    if (_mode_ != FIFO) {
        for (i = 0; i < ALLTYPE; ++i) {
            Queue* q = _queue_ + i;
            Packet* tmp = q->head;
            while (tmp) {
                _sim_.class_remained[tmp->type] += 1;
                tmp = tmp->next;
            }
        }
    }
    else {
        Packet* tmp = _queue_->head;
        while (tmp) {
            _sim_.class_remained[tmp->type] += 1;
            tmp = tmp->next;
        }
    }
    
    if (_server_.packet_in_process) {
        _sim_.class_remained[_server_.packet_in_process->type] += 1;
    }
}

void init(void) {
    _mode_ = FIFO;
    
    _output_ctrl_ = 0;
    
    _next_event_.type = UNKNOWN;
    _next_event_.event_time = 0;
    _next_event_.data_related = NULL;

    _server_.status = IDLE;
    _server_.packet_in_process = NULL;
    _server_.serve_speed = 0;
    
    _sim_.sim_time = 0.0f;
    _sim_.max_sim_time = 0.0f;
    _sim_.total_served_kb = 0.0f;
    _sim_.output_interval = 0;
    int i;
    for (i = 0; i < ALLTYPE; ++i) {
        _sim_.class_arrived[i] = 0;
        _sim_.class_served[i] = 0;
        _sim_.class_served_kb[i] = 0.0f;
        _sim_.class_delayed[i] = 0;
        _sim_.class_dropped[i] = 0;
        _sim_.class_remained[i] = 0;
    }
}


bool append(Queue* que, Packet* p) {
    
    Queue* target_queue = que;
    
    // For SPQ and WFQ, find the right queue to add
    if (_mode_ != FIFO) {
        int i;
        for (i = 0; i < ALLTYPE; ++i) {
            if (p->type == (que + i)->type) {
                target_queue = que + i;
                break;;
            }
        }
    }
    
    // Check if the queue is full
    if ((target_queue->size_kb + p->size_kb) > target_queue->max_size_kb) {
        p->is_dropped = true;
        return false;
    }
    
    // Append the packet to the queue
    if (NULL == target_queue->head) {
        target_queue->head = p;
        target_queue->size_kb = p->size_kb;
    }
    else {
        Packet* current = target_queue->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = p;
        target_queue->size_kb += p->size_kb;
    }
    return true;
}

Queue* get_proper_pop_fifo(void) {
    return _queue_;
}

Queue* get_proper_pop_spq(void) {
    Queue* q = _queue_;
    int i;
    for (i = 0; i < ALLTYPE; ++i) {
        if ((_queue_ + i)->head) {
            q = _queue_ + i;
            break;;
        }
    }
    return q;
}

typedef struct  {
    SOURCE_TYPE type;
    fnum_t off_set;
} OffsetWeight;

int offset_cmp_func(const void* a, const void* b) {
    
    fnum_t oa = ((OffsetWeight*)a)->off_set;
    fnum_t ob = ((OffsetWeight*)b)->off_set;
    
    return oa < ob ? 1 : -1;
}

Queue* get_proper_pop_wfq(void) {
    
    OffsetWeight ow[ALLTYPE];
    
    lfnum_t total_served_kb =  _sim_.total_served_kb <= 0.0f ?  2.0f :  _sim_.total_served_kb;
    int i;
    for (i = 0; i < ALLTYPE; ++i) {
        ow[i].type = i;
        ow[i].off_set = _set_weight_[i] - (_sim_.class_served_kb[i] / total_served_kb);
    }
    
    // Offset sorted by descent
    qsort(ow, ALLTYPE, sizeof(OffsetWeight), offset_cmp_func);
    
    // Find the right queue
    Queue* q = _queue_;
    // Find the right queue
    for (i = 0; i < ALLTYPE; ++i) {
        if (_queue_[ow[i].type].head) {
            q = _queue_ + ow[i].type;
            break;
        }
    }
    return q;
}


// Find the right queue to get packet from
Queue* find_queue(void) {
    Queue* q = _queue_;
    
    switch (_mode_) {
        case SPQ: {
            q = get_proper_pop_spq();
            break;
        }
        case WFQ: {
            q = get_proper_pop_wfq();
            break;
        }
        default:
            break;
    }
    return q;
}

// Get the first packet from the queue
Packet* pop(Queue* queue) {
    Queue* q = find_queue();
    
    // If the queue is empty
    if (!q->head ) {
        return NULL;
    }
    // Pop the packet from the head of the queue
    Packet* p = q->head;
    q->head = p->next;

    // Update the current size of the queue
    q->size_kb -= p->size_kb;
    return p;
}

// Generate packets
void emitter_tick(BitEmitter* em) {
    // Check if there is packet waiting for processing
    if (em->arrival_num > 0) return;

    // Calculate the next packet arrival time
    double arrival_interval = em->packet_size_kb / em->bps;
    double next_arrival_time = em->arraval_time + arrival_interval;
    double off_time_start = em->on_time_start + em->on_time;
    // New packet arrival time is later than the off time, then update the on time
    if (next_arrival_time > off_time_start) {
        em->on_time_start = off_time_start + em->off_time;
        double extra_time_needed = next_arrival_time - off_time_start;
        double new_on_time = expon(em->mean_on);
        double new_off_time = expon(em->mean_off);
        while (new_on_time < extra_time_needed) {
            extra_time_needed -= new_on_time;
            em->on_time_start = em->on_time_start + new_on_time + new_off_time;
            new_on_time = expon(em->mean_on);
            new_off_time = expon(em->mean_off);
        }
        em->on_time = new_on_time;
        em->off_time = new_off_time;
        em->arraval_time = em->on_time_start + extra_time_needed;
    }
    else {
        em->arraval_time = next_arrival_time;
    }
    em->arrival_num = em->concur;
}


Packet* emitter_pop(BitEmitter* em) {
    Packet* p = NULL;
    __MALLOC__(p, sizeof(Packet));
    p->size_kb = em->packet_size_kb;
    p->type = em->type;
    p->next = NULL;
    p->is_dropped = false;
    // Decrease the number of packets
    em->arrival_num--;
    p->arrival_time = em->arraval_time;
    return p;
}


void time_tick(void) {
    // Tick all the source generators
    int i;
    for (i = 0; i < _em_->total_sources; ++i) {
        _em_->tick(_em_+i);
    }
    
    // Find the emitter with the earliest packet arrived
    BitEmitter* em_earliest = _em_;
    for (i = 0; i < _em_->total_sources; ++i) {
        if (em_earliest->arraval_time > _em_[i].arraval_time) {
            em_earliest = _em_ + i;
        }
    }
    
    // Server is Busy and depart time is earlier than next arrival time
    if (BUSY == _server_.status) {
        double serve_duration = _server_.packet_in_process->size_kb / _server_.serve_speed;
        double next_depart_time = _server_.packet_in_process->serve_start_time + serve_duration;
        if (next_depart_time < em_earliest->arraval_time) {
            _next_event_.event_time = next_depart_time;
            _next_event_.type = DEPART;
            return;
        }
    }
    _next_event_.event_time = em_earliest->arraval_time;
    _next_event_.type = ARRIVAL;
    _next_event_.data_related = em_earliest;
}



#pragma expon

double expon(double mean) {
    // Return an exponential random variable with mean time between arrivals of mean.
    return -mean * log(lcgrand(1.0));
}

#pragma depart and arrive
void depart(void) {
    // Finish the current being served packet
    if (_server_.packet_in_process) {
        _server_.packet_in_process->depart_time = _next_event_.event_time;
        _sim_.class_served[ _server_.packet_in_process->type] += 1;
        
        _sim_.class_served_kb[_server_.packet_in_process->type] += _server_.packet_in_process->size_kb;
        _sim_.total_served_kb += _server_.packet_in_process->size_kb;
        __FREE__(_server_.packet_in_process);
    }
    
    // Get one packet from the queue
    Packet* p = _queue_->pop(_queue_);
    if (p) {
        p->serve_start_time = _next_event_.event_time;
        _server_.packet_in_process = p;
        _server_.status = BUSY;
        _sim_.class_delayed[p->type] += 1;
    }
    else {
        _server_.status = IDLE;
    }
}

// Handle the arrival event
void arrive(void) {
    // Add the packet to the queue
    Packet* p = NULL;
    if (_next_event_.data_related) {
        BitEmitter* em = (BitEmitter*)_next_event_.data_related;
        p = em->pop(em);
        p->arrival_time = _next_event_.event_time;
        // Server is busy, add the packet to the queue
        bool success = _queue_->append(_queue_, p);
        // Queue is full, p is dropped.
        _sim_.class_arrived[p->type] += 1;
        if (!success) {
            _sim_.class_dropped[p->type] += 1;
            __FREE__(p);
        }
    }
    
    // If server is idle and get one packet from queue to serve
    if (_server_.status == IDLE) {
        Packet* ps = _queue_->pop(_queue_);
        if (ps) {
            ps->serve_start_time = _next_event_.event_time;
            _server_.status = BUSY;
            _server_.packet_in_process = ps;
        }
        
        if (ps != p) {
            _sim_.class_delayed[ps->type] += 1;
        }
    }
}

#pragma read configuration

#define __CONFIG_MAX_LINE__ 300
#define __ADUIO_SECTION__       "Audio"
#define __VIDEO_SECTION__       "Video"
#define __DATA_SECTION__        "Data"
#define __BIT_RATE_KEY__        "bitrate"
#define __PACKET_SIZE_KEY__     "packet_size"
#define __AVERAGE_ONTIME_KEY__  "average_ontime"
#define __AVERAGE_OFFTIME_KEY__ "average_offtime"
#define __SOURCE_NUMBER_KEY__   "source_number"
#define __QUEUE_SECTION__       "Queue"
#define __MAX_SIZE_KEY__        "max_size"
#define __SERVER_SECTION__      "Server"
#define __SERVE_RATE_KEY__      "serve_rate"
#define __STATIS_SECTION__      "Sim"
#define __STATIS_OUTPUT_INTERVAL__ "output_interval"
#define __STATIS_MAX_SIM_TIME__    "max_sim_time"

#define __SECTION_MATCH_PATTERN__   "[%[^]]"
#define __KEY_VALUE_MATCH_PATTERN__ "%[^ \t\n\r\f\v]%*[ \t\n\r\f\v]=%[^\t\n\r\f\v]"


#define MATCH(s, n) (strcmp(section, s) == 0 && strcmp(key, n) == 0)

int read_config(void) {
    FILE *file = NULL;
    __RMODE_FILE_OPEN__(file, __CONFIG_FILE__);

    char line[__CONFIG_MAX_LINE__];
    char section[__CONFIG_MAX_LINE__];
    char key[__CONFIG_MAX_LINE__];
    char value[__CONFIG_MAX_LINE__];

    int bps[ALLTYPE];
    double packet_size[ALLTYPE];
    float mean_on[ALLTYPE];
    float mean_off[ALLTYPE];
    int total_gen = 0;
    long max_queue_size = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        // Ignore empty lines and comments
        if (line[0] == ';' || line[0] == '\n') {
            continue;
        }
        // Parse sections
        if (line[0] == '[') {
            sscanf(line, __SECTION_MATCH_PATTERN__, section);
            continue;
        }

        // Parse key-value pairs
        if (sscanf(line, __KEY_VALUE_MATCH_PATTERN__, key, value) == 2) {
            printf("Under section: %s", section);
            printf("Key: %s, Value: %s\n", key, value);
        }
        
        // Parse key-value pairs in audio section
        if MATCH(__ADUIO_SECTION__, __BIT_RATE_KEY__) bps[AUDIO] = atoi(value);
        if MATCH(__ADUIO_SECTION__, __PACKET_SIZE_KEY__) packet_size[AUDIO] = atof(value);
        if MATCH(__ADUIO_SECTION__, __AVERAGE_ONTIME_KEY__) mean_on[AUDIO] = atof(value);
        if MATCH(__ADUIO_SECTION__, __AVERAGE_OFFTIME_KEY__) mean_off[AUDIO] = atof(value);

        // Parse key-value pairs in video section
        if MATCH(__VIDEO_SECTION__, __BIT_RATE_KEY__) bps[VIDEO] = atoi(value);
        if MATCH(__VIDEO_SECTION__, __PACKET_SIZE_KEY__) packet_size[VIDEO] = atof(value);
        if MATCH(__VIDEO_SECTION__, __AVERAGE_ONTIME_KEY__) mean_on[VIDEO] = atof(value);
        if MATCH(__VIDEO_SECTION__, __AVERAGE_OFFTIME_KEY__) mean_off[AUDIO] = atof(value);

        // Parse key-value pairs in data section
        if MATCH(__DATA_SECTION__, __BIT_RATE_KEY__) bps[DATA] = atoi(value);
        if MATCH(__DATA_SECTION__, __PACKET_SIZE_KEY__) packet_size[DATA] = atof(value);
        if MATCH(__DATA_SECTION__, __AVERAGE_ONTIME_KEY__) mean_on[DATA] = atof(value);
        if MATCH(__DATA_SECTION__, __AVERAGE_OFFTIME_KEY__) mean_off[AUDIO] = atof(value);

        // Parse key-value pairs in queue section
        if MATCH(__QUEUE_SECTION__, __MAX_SIZE_KEY__) max_queue_size = atoi(value);
        if MATCH(__SERVER_SECTION__, __SERVE_RATE_KEY__) _server_.serve_speed = atoi(value);

        // Parse key-value pairs in statistic section
        if MATCH(__STATIS_SECTION__, __STATIS_OUTPUT_INTERVAL__) _sim_.output_interval = atoi(value);
        if MATCH(__STATIS_SECTION__, __STATIS_MAX_SIM_TIME__) _sim_.max_sim_time = atof(value);
    }
    __CLOSE_FILE__(file); // Close the file
    // Use the parameters read from ini file to initialize the generators
    total_gen = _source_num_[AUDIO] + _source_num_[VIDEO] + _source_num_[DATA];
    // Release the memory of the generators if it is not NULL
    __FREE__(_em_);
    // Allocate memory for the generators
    __MALLOC__(_em_, sizeof(BitEmitter) * total_gen);
    // Initialize all the source generators
    int i = 0, j = 0, assigned = 0;
    for (i = 0; i < ALLTYPE; ++i) {
        for (j = 0; j < _source_num_[i]; ++j) {
            _em_[assigned + j].arraval_time = 0;
            _em_[assigned + j].arrival_num = 0;
            _em_[assigned + j].on_time_start = 0;
            _em_[assigned + j].on_time = expon(mean_on[i]);
            _em_[assigned + j].off_time= expon(mean_off[i]);;
            _em_[assigned + j].mean_on = mean_on[i];
            _em_[assigned + j].mean_off = mean_off[i];
            _em_[assigned + j].bps = bps[i];
            _em_[assigned + j].packet_size_kb = packet_size[i];
            _em_[assigned + j].total_sources = total_gen;
            _em_[assigned + j].type = i;
            _em_[assigned + j].pop = emitter_pop;
            _em_[assigned + j].tick = emitter_tick;
            _em_[assigned + j].concur = 1; // One packet is generated at the same time
        }
        assigned += _source_num_[i];
    }
    
    __FREE__(_queue_);
    
    // Single queue
    if (_mode_ == FIFO) {
        __MALLOC__(_queue_, sizeof(Queue));
        _queue_->max_size_kb = max_queue_size;
        _queue_->type = ALLTYPE;
        _queue_->size_kb = 0;
        _queue_->head = NULL;
        _queue_->append = append;
        _queue_->pop = pop;
    }
    else { // Three queues
        __MALLOC__(_queue_, sizeof(Queue) * ALLTYPE);
        for (i = 0; i < ALLTYPE; ++i) {
            Queue* t = _queue_+i;
            t->type = i;
            t->max_size_kb = max_queue_size * 1.0 / ALLTYPE;
            t->size_kb = 0;
            t->append = append;
            t->pop = pop;
            t->head = NULL;
        }
    }
    return 0;
}
