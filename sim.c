
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sim.h"

/**************************************************************************************************/

System    gsys_;
Event     next_event_;
PackGo*   gpg_;
GConfig   gc_;
Collector cll_;

int main(int argc, char const *argv[]) 
{
    init();
    
    read_params();
    
    srand((unsigned)time(NULL));
    
    while (cll_.ctime <= cll_.max_time)
    {
        print_contrl();
        
        time_tick();
        
        if (next_event_.type == Arrival)
            arrive();
        else if (next_event_.type == Departure)
            depart();
        else
            printf("--------------Event type not matched------------");
    }
    return 0;
}


/**************************************************************************************************/

#define SEC_KEY_CMP(s, k) if(strcmp(section, s) == 0 && strcmp(key, k) == 0)

int read_params(void)
{
    FILE* f = fopen("input.ini", "r");
    if( !f ) {
       fprintf(stderr, "Failed to open file '%s' \n", "input.ini");
       exit(EXIT_FAILURE);
    }
    
    const int max_line = 200, type_num = 3;
    char line[max_line];
    char section[max_line];
    char key[max_line];
    char value[max_line];
    
    int bps[type_num];
    double pack_len[type_num];
    float mean_on[type_num];
    float mean_off[type_num];
    int source_num[type_num];
    
    long max_queue_len = 0;
    
    while (fgets(line, sizeof(line), f) != NULL) {
        if (line[0] == ';' || line[0] == '\n') {
            continue;
        }
        
        if (line[0] == '[') {
            sscanf(line, "[%[^]]", section);
            continue;
        }

        if (sscanf(line,"%[^ \t\n\r\f\v]%*[ \t\n\r\f\v]=%[^\t\n\r\f\v]", key, value) == 2) {
            printf("Section: %s, key: %s, value: %s", section, key, value);
        }
        // Audio
        SEC_KEY_CMP("Audio", "bps")          bps[Audio] = atoi(value);
        SEC_KEY_CMP("Audio", "size")         pack_len[Audio] = atof(value);
        SEC_KEY_CMP("Audio", "mean_on")      mean_on[Audio] = atof(value);
        SEC_KEY_CMP("Audio", "mean_off")     mean_off[Audio] = atof(value);
        SEC_KEY_CMP("Audio", "num")          source_num[Audio] = atoi(value);
        SEC_KEY_CMP("Audio", "weight")       gc_.weight_sets[Audio] = atof(value);
        // Video
        SEC_KEY_CMP("Video", "bps")          bps[Video] = atoi(value);
        SEC_KEY_CMP("Video", "size")         pack_len[Video] = atof(value);
        SEC_KEY_CMP("Video", "mean_on")      mean_on[Video] = atof(value);
        SEC_KEY_CMP("Video", "mean_off")     mean_off[Video] = atof(value);
        SEC_KEY_CMP("Video", "num")          source_num[Video] = atoi(value);
        SEC_KEY_CMP("Video", "weight")       gc_.weight_sets[Video] = atof(value);
        // Data
        SEC_KEY_CMP("Data", "bps")           bps[Data] = atoi(value);
        SEC_KEY_CMP("Data", "size")          pack_len[Data] = atof(value);
        SEC_KEY_CMP("Data", "mean_on")       mean_on[Data] = atof(value);
        SEC_KEY_CMP("Data", "mean_off")      mean_off[Data] = atof(value);
        SEC_KEY_CMP("Data", "num")           source_num[Data] = atoi(value);
        SEC_KEY_CMP("Data", "weight")        gc_.weight_sets[Data] = atof(value);
        // Queue
        SEC_KEY_CMP("Queue", "max_size")     max_queue_len = atoi(value);
        // Server
        SEC_KEY_CMP("Server", "process_rate")       gsys_.server.serve_rate = atoi(value);
        // Simulation
        SEC_KEY_CMP("Simulation", "print_interval") cll_.out_interval = atoi(value);
        SEC_KEY_CMP("Simulation", "sim_time")       cll_.max_time = atof(value);
        SEC_KEY_CMP("Simulation", "mode")           gc_.qmode = atoi(value);
    }
    
    if (f) {
        fclose(f);
        f = NULL;
    }

    gsys_.queue = (Queue*)(gc_.qmode == QFIFO ? malloc(sizeof(Queue)) : malloc(sizeof(Queue) * type_num));
    

    
    // Initialize all the source generator
    int i, total_source = 0;
    for (i = 0; i < type_num; ++i) total_source += source_num[i];
    gpg_ = (PackGo*)malloc(sizeof(PackGo) * total_source);

    int i = 0, j = 0, assigned = 0;
    for (i = 0; i < ALLTYPE; ++i) {
        for (j = 0; j < _source_num_[i]; ++j) {
            _em_[assigned + j].arrival_time = expon(mean_off[i]);
            _em_[assigned + j].arrival_num = 0;
            _em_[assigned + j].on_time_start = _em_[assigned + j].arrival_time;
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

double expon(double mean) 
{
    return -mean * log(rand(100) * 1.0f / 100);
}

void print_contrl(void) 
{
    if (cll_.internal_acc >= cll_.output_interval) {
        packet_left_in_system();
        print_sim_data();
        cll_.internal_acc = 0;
    }
    else {
        cll_.internal_acc += 1;
    }
}

void init(void) 
{
    gpg_ = NULL;
    
    gc_.qmode = QFIFO;
    
    cll_.internal_acc = 0;
    
    next_event_.type = Initialized;
    next_event_.time = 0.0f;
    next_event_.data = NULL;

    gsys_.server.state = Idle;
    gsys_.server.pack_in = NULL;
    gsys_.server.serve_rate = 0;
    
    
    int i = 0;
    const int type_nums = 3;
    while (i < type_nums) {
        cll_.arrived[i] = 0;
        cll_.served[i] = 0;
        cll_.delayed[i] = 0;
        cll_.dropped[i] = 0;
        cll_.left_in_system[i] = 0;
        cll_.res_time[i] = 0.0f;
        cll_.delay_time[i] = 0.0f;
        ++i;
    }
    cll_.ctime = 0.0f;
    cll_.max_time = 0.0f;
    
    cll_.out_interval = 0;
    cll_.internal_acc = 0;
}

// Calculate the length of queue for each category
void accumulated_queue_length(void)
{
    if (_mode_ == FIFO) {
        _sim_.class_queue_len[AUDIO] += _queue_->size_kb;
    }
    else {
        int i;
        for (i = 0; i < ALLTYPE; ++i) {
            Queue* q = _queue_ + i;
            _sim_.class_queue_len[q->type] += q->size_kb;
        }
    }
}

void statistics_print(void) 
{
    
    char* mode = "FIFO";
    if (_mode_ == SPQ) {
        mode = "Strict Priority Queue";
    }
    else if (_mode_== FIFO) {
        mode = "FIFO";
    }
    else {
        mode = "Weight Fair Queue";
    }
    
    printf("----------Mode: %s---------Sim time: %.4f-----------------\n",
           mode, _sim_.sim_time);
    
    printf("Total served packet in kb: %0.4Lf \n", _sim_.total_served_kb);
    
    
    lfnum_t totoal_served_kb = _sim_.total_served_kb <= 0 ? 10 : _sim_.total_served_kb;
    printf("Percentage of Served (audio, video, data): %0.4Lf, %0.4Lf, %0.4Lf \n",
           _sim_.class_served_kb[AUDIO] / totoal_served_kb,
           _sim_.class_served_kb[VIDEO] / totoal_served_kb,
           _sim_.class_served_kb[DATA] / totoal_served_kb);

    
    if (_mode_ != FIFO) {
        
        printf("Packet arrived (audio, video, data): %ld, %ld, %ld \n",
               _sim_.class_arrived[AUDIO],
               _sim_.class_arrived[VIDEO],
               _sim_.class_arrived[DATA]);
        
        
        printf("Packet served (audio, video, data): %ld, %ld, %ld \n",
               _sim_.class_served[AUDIO],
               _sim_.class_served[VIDEO],
               _sim_.class_served[DATA]);
        
        printf("Packet delayed (audio, video, data): %ld, %ld, %ld \n",
               _sim_.class_delayed[AUDIO],
               _sim_.class_delayed[VIDEO],
               _sim_.class_delayed[DATA]);
        
        printf("Package dropped (audio, video, data): %ld, %ld, %ld \n",
               _sim_.class_dropped[AUDIO],
               _sim_.class_dropped[VIDEO],
               _sim_.class_dropped[DATA]);
        
        printf("Packet remained (audio, video, data): %ld, %ld, %ld \n",
               _sim_.class_remained[AUDIO],
               _sim_.class_remained[VIDEO],
               _sim_.class_remained[DATA]);
        
        
        printf("Packet served in kb (audio, video, data): %0.4Lf, %0.4Lf, %0.4Lf \n",
               _sim_.class_served_kb[AUDIO],
               _sim_.class_served_kb[VIDEO],
               _sim_.class_served_kb[DATA]);
        
        lfnum_t average_delay[ALLTYPE] = {
            _sim_.class_delay_time[AUDIO] / (_sim_.class_delayed[AUDIO] <= 0 ? 10 : _sim_.class_delayed[AUDIO]),
            _sim_.class_delay_time[VIDEO] / (_sim_.class_delayed[VIDEO] <= 0 ? 10 : _sim_.class_delayed[VIDEO]),
            _sim_.class_delay_time[DATA] / (_sim_.class_delayed[DATA] <= 0 ? 10 : _sim_.class_delayed[DATA])
        };
        
        printf("Average delayed time (audio, video, data) : %.4Lf, %.4Lf, %.4Lf \n",
               average_delay[AUDIO],
               average_delay[VIDEO],
               average_delay[DATA]);
        
        
        printf("Packet blocking ratio (audio, video, data) :  %.4Lf, %.4Lf, %.4Lf \n",
               _sim_.class_dropped[AUDIO] * 1.0l / _sim_.class_arrived[AUDIO],
               _sim_.class_dropped[VIDEO] * 1.0l / _sim_.class_arrived[VIDEO],
               _sim_.class_dropped[DATA] * 1.0l/ _sim_.class_arrived[DATA]);
        
        
        printf("Average response time (audio, video, data): %.4Lf, %.4Lf, %.4Lf \n",
               _sim_.class_res_time[AUDIO] / _sim_.class_served[AUDIO],
               _sim_.class_res_time[VIDEO] / _sim_.class_served[VIDEO],
               _sim_.class_res_time[DATA] / _sim_.class_served[DATA]);
        
        printf("Response time (audio, video, data): %.4Lf, %.4Lf, %.4Lf \n",
               _sim_.class_res_time[AUDIO],
               _sim_.class_res_time[VIDEO],
               _sim_.class_res_time[DATA]);
        
        
        printf("Average Queue Length in KB (audio, video, data ): %.4Lf, %.4Lf, %.4Lf \n",
               _sim_.class_queue_len[AUDIO] / _sim_.event_num,
               _sim_.class_queue_len[VIDEO] / _sim_.event_num,
               _sim_.class_queue_len[DATA] / _sim_.event_num);
        
        
    }
    else {
        printf("Packet served : %ld \n",
               _sim_.class_served[AUDIO] + _sim_.class_served[VIDEO] + _sim_.class_served[DATA]);
        
        printf("Packet arrived : %ld \n",
               _sim_.class_arrived[AUDIO] + _sim_.class_arrived[VIDEO] + _sim_.class_arrived[DATA]);
        
        printf("Packet delayed : %ld \n",
               _sim_.class_delayed[AUDIO] + _sim_.class_delayed[VIDEO] + _sim_.class_delayed[DATA]);
        
        printf("Package dropped : %ld \n",
               _sim_.class_dropped[AUDIO] + _sim_.class_dropped[VIDEO] + _sim_.class_dropped[DATA]);
        
        printf("Packet remained: %ld \n",
               _sim_.class_remained[AUDIO] + _sim_.class_remained[VIDEO] + _sim_.class_remained[DATA]);
        
        
        num_t total_dropped = _sim_.class_dropped[AUDIO] + _sim_.class_dropped[VIDEO] + _sim_.class_dropped[DATA];
        num_t total_arrived = _sim_.class_arrived[AUDIO] + _sim_.class_arrived[VIDEO] + _sim_.class_arrived[DATA];
        printf("Packet blocking ratio :  %.4Lf \n", total_dropped * 1.0l / total_arrived);
        
        num_t total_packet_delayed = _sim_.class_delayed[AUDIO] + _sim_.class_delayed[VIDEO] + _sim_.class_delayed[DATA];
        
        lfnum_t average_delayed_time = total_packet_delayed <= 0 ? 0 :_sim_.total_delay_time / total_packet_delayed;
        
        printf("Delayed time: %0.4Lf, average dalayed time: %0.4Lf \n",
               _sim_.total_delay_time, average_delayed_time);
        
    
        lfnum_t total_res_time = _sim_.class_res_time[AUDIO] + _sim_.class_res_time[VIDEO] + _sim_.class_res_time[DATA];
        num_t total_packet_servedd = _sim_.class_served[AUDIO] + _sim_.class_served[VIDEO] + _sim_.class_served[DATA];
        printf("Average Response time (total): %.4Lf \n", total_res_time / total_packet_servedd);
        
        printf("Response time (audio, video, data): %.4Lf \n",total_res_time);
        
        printf("Average Queue Length in KB: %.4Lf \n",
               _sim_.class_queue_len[AUDIO] / _sim_.event_num);
        
    }
    printf("\n");
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

bool append(Queue* que, Packet* p) 
{
    
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
    double next_arrival_time = em->arrival_time + arrival_interval;
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
        em->arrival_time = em->on_time_start + extra_time_needed;
    }
    else {
        em->arrival_time = next_arrival_time;
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
    p->arrival_time = em->arrival_time;
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
        if (em_earliest->arrival_time > _em_[i].arrival_time) {
            em_earliest = _em_ + i;
        }
    }
    
    // Server is Busy and depart time is earlier than next arrival time
    if (BUSY == _server_.status) {
        double serve_duration = _server_.packet_in_process->size_kb / _server_.serve_speed;
        double next_depart_time = _server_.packet_in_process->serve_start_time + serve_duration;
        if (next_depart_time < em_earliest->arrival_time) {
            _next_event_.event_time = next_depart_time;
            _next_event_.type = DEPART;
            return;
        }
    }
    _next_event_.event_time = em_earliest->arrival_time;
    _next_event_.type = ARRIVAL;
    _next_event_.data_related = em_earliest;
    
    cll_.ctime = next_event_.time;
}




#pragma depart and arrive
void depart(void) {
    // Finish the current being served packet
    if (_server_.packet_in_process) {
        _server_.packet_in_process->depart_time = _next_event_.event_time;
        _sim_.class_served[ _server_.packet_in_process->type] += 1;
        
        _sim_.class_served_kb[_server_.packet_in_process->type] += _server_.packet_in_process->size_kb;
        _sim_.total_served_kb += _server_.packet_in_process->size_kb;
        
        lfnum_t res_time = _server_.packet_in_process->depart_time - _server_.packet_in_process->arrival_time;
        
        _sim_.class_res_time[_server_.packet_in_process->type] += res_time;
        
        __FREE__(_server_.packet_in_process);
    }
    
    // Get one packet from the queue
    Packet* p = _queue_->pop(_queue_);
    if (p) {
        p->serve_start_time = _next_event_.event_time;
        _server_.packet_in_process = p;
        _server_.status = BUSY;
        _sim_.class_delayed[p->type] += 1;
        _sim_.total_delay_time += (p->serve_start_time - p->arrival_time);
        _sim_.class_delay_time[p->type] += (p->serve_start_time - p->arrival_time);
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
            _sim_.total_delay_time += (p->serve_start_time - p->arrival_time);
            _sim_.class_delay_time[p->type] += (p->serve_start_time - p->arrival_time);
        }
    
        // Packets popped and pushed are not the same one.
        // Therefore, there is a delay happened.
        if (ps != p) {
            _sim_.class_delayed[ps->type] += 1;
        }
    }
}
