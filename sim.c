
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
    time_t t;
    
    srand((unsigned)time(&t));
    
    init();
    
    read_params();
    
    while (cll_.ctime <= cll_.max_time)
    {
        print_contrl();
        
        time_tick();
        
        if (next_event_.type == EArrive)
            arrive();
        else if (next_event_.type == EDepart)
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
    
    const int max_line = 200;
    const int type_num = 3;
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
        SEC_KEY_CMP("Simulation", "mode")           gc_.qmode = (QueueMode)atoi(value);
    }
    
    if (f) fclose(f); f = NULL;
    
    gsys_.queue = (Queue*)(gc_.qmode == QFIFO ? malloc(sizeof(Queue)) : malloc(sizeof(Queue) * type_num));
    
    // Initialize all the source generator
    int i, total_source = 0;
    for (i = 0; i < type_num; ++i) total_source += source_num[i];
    gpg_ = (PackGo*)malloc(sizeof(PackGo) * total_source);
    
    // Init for bit generator
    int j, offset = 0;
    for (i = 0; i < type_num; ++i) 
    {
        for (j = 0; j < source_num[i]; ++j) 
        {
            PackGo* g = gpg_ + offset + j;
            g->pack_num = 0;
            g->on_start = expon(mean_off[i]);
            g->time_go = g->on_start;
            g->mean_on = mean_on[i];
            g->mean_off = mean_off[i];
            g->on_length = expon(g->mean_on);
            g->off_length = expon(g->mean_off);
            g->bps = bps[i];
            g->size_byte = pack_len[i];
            g->total_goers = total_source;
            g->type = (SourceType)i;
        }
        offset += source_num[i];
    }
    
    int queue_num = gc_.qmode == QFIFO ? 1 : type_num;
    gsys_.queue = (Queue*)malloc(sizeof(Queue) * queue_num);
    
    for (i = 0; i < queue_num; ++i) 
    {
        Queue* q = gsys_.queue + i;
        q->max_len = max_queue_len;
        q->type = (SourceType)i;
        q->len = 0;
        q->pack_head = NULL;
    }
    return 0;
}

double expon(double mean) 
{
    return -mean * log(rand() * 1.0f / RAND_MAX);
}

void print_contrl(void) 
{
    if (cll_.internal_acc >= cll_.out_interval) 
    {
        pack_left_in_sys();
        statistics_print();
        cll_.internal_acc = 0;
    }
    else 
    {
        cll_.internal_acc += 1;
    }
}

void init(void) 
{
    gpg_ = NULL;
    
    gc_.qmode = QFIFO;
    
    cll_.internal_acc = 0;
    
    next_event_.type = EInitialized;
    next_event_.time = 0.0f;
    next_event_.data = NULL;

    gsys_.server.state = Idle;
    gsys_.server.pack_in = NULL;
    gsys_.server.serve_rate = 0;
    gsys_.queue = NULL;
    
    int i = 0;
    const int type_nums = 3;
    while (i < type_nums) 
    {
        cll_.arrived[i] = 0;
        cll_.served[i] = 0;
        cll_.delayed[i] = 0;
        cll_.dropped[i] = 0;
        cll_.left_in_sys[i] = 0;
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
    int queue_num = gc_.qmode == QFIFO ? 1 : 3;
    int i = 0;
    while (i < queue_num) 
    {
        Queue* q = gsys_.queue + i;
        cll_.queue_len_acc[q->type] += q->len;
        ++i;
    }
}

#define FIFO_OUT_FILE   "fifo.txt"
#define SPQ_OUT_FILE    "spq.txt"
#define WFQ_OUT_FILE    "wfq.txt"

void statistics_print(void)
{
    
//    char* mode = "FIFO";
//    if (_mode_ == SPQ) {
//        mode = "Strict Priority Queue";
//    }
//    else if (_mode_== FIFO) {
//        mode = "FIFO";
//    }
//    else {
//        mode = "Weight Fair Queue";
//    }
//    
//    printf("----------Mode: %s---------Sim time: %.4f-----------------\n",
//           mode, _sim_.sim_time);
//    
//    printf("Total served packet in kb: %0.4Lf \n", _sim_.total_served_kb);
//    
//    
//    lfnum_t totoal_served_kb = _sim_.total_served_kb <= 0 ? 10 : _sim_.total_served_kb;
//    printf("Percentage of Served (audio, video, data): %0.4Lf, %0.4Lf, %0.4Lf \n",
//           _sim_.class_served_kb[AUDIO] / totoal_served_kb,
//           _sim_.class_served_kb[VIDEO] / totoal_served_kb,
//           _sim_.class_served_kb[DATA] / totoal_served_kb);
//
//    
//    if (_mode_ != FIFO) {
//        
//        printf("Packet arrived (audio, video, data): %ld, %ld, %ld \n",
//               _sim_.class_arrived[AUDIO],
//               _sim_.class_arrived[VIDEO],
//               _sim_.class_arrived[DATA]);
//        
//        
//        printf("Packet served (audio, video, data): %ld, %ld, %ld \n",
//               _sim_.class_served[AUDIO],
//               _sim_.class_served[VIDEO],
//               _sim_.class_served[DATA]);
//        
//        printf("Packet delayed (audio, video, data): %ld, %ld, %ld \n",
//               _sim_.class_delayed[AUDIO],
//               _sim_.class_delayed[VIDEO],
//               _sim_.class_delayed[DATA]);
//        
//        printf("Package dropped (audio, video, data): %ld, %ld, %ld \n",
//               _sim_.class_dropped[AUDIO],
//               _sim_.class_dropped[VIDEO],
//               _sim_.class_dropped[DATA]);
//        
//        printf("Packet remained (audio, video, data): %ld, %ld, %ld \n",
//               _sim_.class_remained[AUDIO],
//               _sim_.class_remained[VIDEO],
//               _sim_.class_remained[DATA]);
//        
//        
//        printf("Packet served in kb (audio, video, data): %0.4Lf, %0.4Lf, %0.4Lf \n",
//               _sim_.class_served_kb[AUDIO],
//               _sim_.class_served_kb[VIDEO],
//               _sim_.class_served_kb[DATA]);
//        
//        lfnum_t average_delay[ALLTYPE] = {
//            _sim_.class_delay_time[AUDIO] / (_sim_.class_delayed[AUDIO] <= 0 ? 10 : _sim_.class_delayed[AUDIO]),
//            _sim_.class_delay_time[VIDEO] / (_sim_.class_delayed[VIDEO] <= 0 ? 10 : _sim_.class_delayed[VIDEO]),
//            _sim_.class_delay_time[DATA] / (_sim_.class_delayed[DATA] <= 0 ? 10 : _sim_.class_delayed[DATA])
//        };
//        
//        printf("Average delayed time (audio, video, data) : %.4Lf, %.4Lf, %.4Lf \n",
//               average_delay[AUDIO],
//               average_delay[VIDEO],
//               average_delay[DATA]);
//        
//        
//        printf("Packet blocking ratio (audio, video, data) :  %.4Lf, %.4Lf, %.4Lf \n",
//               _sim_.class_dropped[AUDIO] * 1.0l / _sim_.class_arrived[AUDIO],
//               _sim_.class_dropped[VIDEO] * 1.0l / _sim_.class_arrived[VIDEO],
//               _sim_.class_dropped[DATA] * 1.0l/ _sim_.class_arrived[DATA]);
//        
//        
//        printf("Average response time (audio, video, data): %.4Lf, %.4Lf, %.4Lf \n",
//               _sim_.class_res_time[AUDIO] / _sim_.class_served[AUDIO],
//               _sim_.class_res_time[VIDEO] / _sim_.class_served[VIDEO],
//               _sim_.class_res_time[DATA] / _sim_.class_served[DATA]);
//        
//        printf("Response time (audio, video, data): %.4Lf, %.4Lf, %.4Lf \n",
//               _sim_.class_res_time[AUDIO],
//               _sim_.class_res_time[VIDEO],
//               _sim_.class_res_time[DATA]);
//        
//        
//        printf("Average Queue Length in KB (audio, video, data ): %.4Lf, %.4Lf, %.4Lf \n",
//               _sim_.class_queue_len[AUDIO] / _sim_.event_num,
//               _sim_.class_queue_len[VIDEO] / _sim_.event_num,
//               _sim_.class_queue_len[DATA] / _sim_.event_num);
//        
//        
//    }
//    else {
//        printf("Packet served : %ld \n",
//               _sim_.class_served[AUDIO] + _sim_.class_served[VIDEO] + _sim_.class_served[DATA]);
//        
//        printf("Packet arrived : %ld \n",
//               _sim_.class_arrived[AUDIO] + _sim_.class_arrived[VIDEO] + _sim_.class_arrived[DATA]);
        
//        printf("Packet delayed : %ld \n",
//               _sim_.class_delayed[AUDIO] + _sim_.class_delayed[VIDEO] + _sim_.class_delayed[DATA]);
//        
//        printf("Package dropped : %ld \n",
//               _sim_.class_dropped[AUDIO] + _sim_.class_dropped[VIDEO] + _sim_.class_dropped[DATA]);
//        
//        printf("Packet remained: %ld \n",
//               _sim_.class_remained[AUDIO] + _sim_.class_remained[VIDEO] + _sim_.class_remained[DATA]);
//        
//        
//        num_t total_dropped = _sim_.class_dropped[AUDIO] + _sim_.class_dropped[VIDEO] + _sim_.class_dropped[DATA];
//        num_t total_arrived = _sim_.class_arrived[AUDIO] + _sim_.class_arrived[VIDEO] + _sim_.class_arrived[DATA];
//        printf("Packet blocking ratio :  %.4Lf \n", total_dropped * 1.0l / total_arrived);
//        
//        num_t total_packet_delayed = _sim_.class_delayed[AUDIO] + _sim_.class_delayed[VIDEO] + _sim_.class_delayed[DATA];
//        
//        lfnum_t average_delayed_time = total_packet_delayed <= 0 ? 0 :_sim_.total_delay_time / total_packet_delayed;
//        
//        printf("Delayed time: %0.4Lf, average dalayed time: %0.4Lf \n",
//               _sim_.total_delay_time, average_delayed_time);
//        
//    
//        lfnum_t total_res_time = _sim_.class_res_time[AUDIO] + _sim_.class_res_time[VIDEO] + _sim_.class_res_time[DATA];
//        num_t total_packet_servedd = _sim_.class_served[AUDIO] + _sim_.class_served[VIDEO] + _sim_.class_served[DATA];
//        printf("Average Response time (total): %.4Lf \n", total_res_time / total_packet_servedd);
//        
//        printf("Response time (audio, video, data): %.4Lf \n",total_res_time);
//        
//        printf("Average Queue Length in KB: %.4Lf \n",
//               _sim_.class_queue_len[AUDIO] / _sim_.event_num);
//        
//    }
    printf("\n");
}

void pack_left_in_sys(void)
{
    int i;
    int queue_num = gc_.qmode == QFIFO ? 1 : 3;
    for (i = 0; i < queue_num; ++i) 
    {
        // Set to zero first
        cll_.left_in_sys[i] = 0;
        
        Queue* qu = gsys_.queue + i;
        BitPack* tmp = qu->pack_head;
        while (tmp) 
        {
            cll_.left_in_sys[tmp->type] += 1;
            tmp = tmp->next;
        }
    }
    
    if (gsys_.server.pack_in)
    {
        cll_.left_in_sys[gsys_.server.pack_in->type] += 1;
    }
}

int add_packet_to_queue(BitPack* p)
{
    Queue* target_to_add = gsys_.queue;
    if (gc_.qmode != QFIFO) 
    {
        int i;
        const int type_num = 3;
        for (i = 0; i < type_num; ++i)
        {
            if (p->type == gsys_.queue[i].type) 
            {
                target_to_add = gsys_.queue + i;
                break;
            }
        }
    }
    
    // Queue reaches its capacity
    if (target_to_add->len == target_to_add->max_len) 
    {
        p->dropped = 1;
        return 0;
    }
    
    // Queue is empty
    if (NULL == target_to_add->pack_head) 
    {
        target_to_add->pack_head = p;
        target_to_add->len = 1;
    }
    else 
    {
        BitPack* cur = target_to_add->pack_head;
        while (cur->next != NULL) 
        {
            cur = cur->next;
        }
        cur->next = p;
        target_to_add->len += 1;
    }
    return 1;
}

// Comparsion func for qsort
int abs_cmp(const void* aw1, const void* aw2) {
    float a = ((AbsWeight*)aw1)->abs_weight;
    float b = ((AbsWeight*)aw2)->abs_weight;
    return (a < b ? 1 : -1);
}

Queue* find_queue_of_wfq(void) {
    const int type_num = 3;
    
    AbsWeight off_weight[type_num];
    
    long double served_total_byte = cll_.total_served_byte;
    if (cll_.served <= 0) served_total_byte = 1.0f;
    
    int i;
    for (i = 0; i < type_num; ++i) 
    {
        off_weight[i].type = (SourceType)i;
        off_weight[i].abs_weight = gc_.weight_sets[i] - (cll_.served_byte[i] / served_total_byte);
    }
    
    // Sorted by descent
    qsort(off_weight, type_num, sizeof(AbsWeight), abs_cmp);
    
    Queue* q = gsys_.queue;
    while (i < type_num) 
    {
        Queue* cq = q + off_weight[i].type;
        if (cq->pack_head) 
        {
            q = cq;
            break;
        }
        ++i;
    }
    return q;
}

Queue* find_right_queue(void)
{
    Queue* q = gsys_.queue;
    int i;
    const int type_num = 3;
    if (QFIFO == gc_.qmode)
        return gsys_.queue;
    else if (QSPQ == gc_.qmode) 
    {
        for (i = 0; i < type_num; ++i)
            if (q[i].pack_head)
                return q + i;
    }
    else
        q = find_queue_of_wfq();
    return q;
}

// Remove the first packet in the queue
BitPack* get_pack_from_queue(void) 
{
    Queue* q = find_right_queue();
    if (NULL == q->pack_head )  return NULL;
    
    BitPack* pack = q->pack_head;
    q->pack_head = pack->next;

    q->len -= 1;
    return pack;
}

// Generate packets
void packgo_tick(PackGo* pg) {
    if (pg->pack_num > 0) return;
    
    // Generate one packet
    pg->pack_num = 1;
    
    // Calculate the next time to arrive
    double arri_interval = pg->size_byte / pg->bps;
    double next_arrival_time = pg->time_go + arri_interval;
    
    double off_start = pg->on_start + pg->on_length;
    if (next_arrival_time > off_start) 
    {
        pg->on_start = off_start + pg->off_length;
        double extra_time_needed = next_arrival_time - off_start;
        double new_on_length = expon(pg->mean_on);
        double new_off_length = expon(pg->mean_off);
        while (new_on_length < extra_time_needed) 
        {
            extra_time_needed -= new_on_length;
            pg->on_start = pg->on_start + new_on_length + new_off_length;
            new_on_length = expon(pg->mean_on);
            new_off_length = expon(pg->mean_off);
        }
        pg->on_length = new_on_length;
        pg->off_length = new_off_length;
        pg->time_go = pg->on_start + extra_time_needed;
    }
    else 
    {
        pg->time_go = next_arrival_time;
    }
}

BitPack* send_pack_to_arrive(PackGo* pg) {
    BitPack* p = (BitPack*)malloc(sizeof(BitPack));
    p->size_byte = pg->size_byte;
    p->time_arrival = pg->time_go;
    p->type = pg->type;
    p->dropped = 0;
    p->next = NULL;
    pg->pack_num -= 1;
    return p;
}

void time_tick(void) 
{
    int i;
    PackGo* earliest = gpg_;
    for (i = 0; i < gpg_->total_goers; ++i) 
    {
        packgo_tick(gpg_ + i);
        if (earliest->time_go > gpg_[i].time_go) 
        {
            earliest = gpg_ + i;
        }
    }
    
    if (Busy == gsys_.server.state) 
    {
        double serve_len = gsys_.server.pack_in->size_byte / gsys_.server.serve_rate;
        double next_depart = gsys_.server.pack_in->time_serve + serve_len;
        if (next_depart < earliest->time_go)
        {
            next_event_.time = next_depart;
            next_event_.type = EDepart;
            return;
        }
    }
    next_event_.time = earliest->time_go;
    next_event_.type = EArrive;
    next_event_.data = earliest;
    
    // Set current simlation time to this event time
    cll_.ctime = next_event_.time;
}

#pragma depart and arrive
void depart(void) 
{
    Server* serv = &gsys_.server;
    if (Busy == serv->state) 
    {
        serv->pack_in->time_depart = next_event_.time;
        cll_.served[serv->pack_in->type] += 1;
        cll_.served_byte[serv->pack_in->type] += serv->pack_in->size_byte;
        cll_.total_served_byte += serv->pack_in->size_byte;
                
        cll_.area_time[serv->pack_in->type] += (serv->pack_in->time_depart - serv->pack_in->time_arrival);
        cll_.total_area_time += (serv->pack_in->time_depart - serv->pack_in->time_arrival);
        
        free(serv->pack_in);
        serv->pack_in = NULL;
    }
    
    gsys_.server.state = Idle;
    
    BitPack* p = get_pack_from_queue();
    if (p) 
    {
        cll_.delayed[p->type] += 1;
        p->time_serve = next_event_.time;
        gsys_.server.pack_in = p;
        gsys_.server.state = Busy;
        
        cll_.total_delay_time += (p->time_serve - p->time_arrival);
        cll_.delay_time[p->type] += (p->time_serve - p->time_arrival);
    }
}

// Handle the arrival event
void arrive(void) 
{
    if (next_event_.data) {
        BitPack* pack = send_pack_to_arrive((PackGo*)next_event_.data);
        pack->time_arrival = next_event_.time;
        
        int res = add_packet_to_queue(pack);
        
        if (res) 
        {
            cll_.arrived[pack->type] += 1;
        }
        else 
        {
            cll_.dropped[pack->type] += 1;
            free(pack);
        }
    }
    
    if (gsys_.server.state == Idle) 
    {
        BitPack* bp = get_pack_from_queue();
        if (bp) 
        {
            gsys_.server.state = Busy;
            gsys_.server.pack_in = bp;
            gsys_.server.pack_in->time_serve = next_event_.time;
        }
    }
}
