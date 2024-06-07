
#ifndef __SIMULATE_H__
#define __SIMULATE_H__

typedef enum {
    Audio = 0,
    Video,
    Data,
    Mixed
} SourceType;

typedef enum {
    QFIFO = 0,
    QSPQ,
    QWFQ
} QueueMode;

typedef enum {
    EArrive = 0,
    EDepart,
    EInitialized
} EventType;

typedef struct BitPack {
    SourceType    type;
    unsigned long size_byte;
    double        time_arrival;
    double        time_depart;
    double        time_serve;
    int           dropped; // 0 - not dropped, 1 - dropped
    struct        BitPack* next;
} BitPack;

typedef struct  {
    SourceType    type;
    BitPack*      pack_head;
    unsigned long max_len;
    unsigned long len;
} Queue;

typedef struct {
    EventType type;
    double    time;
    void*     data;
} Event;

typedef enum {
    Idle = 0,
    Busy
} ServerState;

typedef struct  {
    ServerState state;
    BitPack*    pack_in;
    float       serve_rate;
} Server;

typedef struct {
    Queue* queue;
    Server server;
} System;


typedef struct  {
    SourceType type;
    double mean_on;
    double mean_off;
    double bps;
    double size_byte;
    
    double on_start;
    double on_length;
    double off_length;
    int    pack_num;
    double time_go;
    long   total_goers;
} PackGo;


typedef struct  {
    unsigned long  arrived[3];
    double         ctime; // current time
    unsigned long  served[3];
    unsigned long  delayed[3];
    unsigned long  dropped[3];
    unsigned long  left_in_sys[3];
    long           out_interval;
    long           internal_acc;
    double         max_time;
    long double    delay_time[3];
    long double    total_delay_time;
    unsigned long long  total_served_byte;
    unsigned long long  served_byte[3];
    long long      queue_len_acc[3];
    long double    area_time[3];
    long double    total_area_time;
} Collector;


typedef struct {
    QueueMode qmode;          // Queuing mode
    float     weight_sets[3]; // Weights for weight fair queuing
    int       sources[3];     // The number of sources in each type
} GConfig;


typedef struct  {
    SourceType type;
    float      abs_weight;
} AbsWeight;

double expon(double);
void   print_contrl(void);
void   init(void);
int    read_params(void);
void   depart(void);
void   arrive(void);
void   time_tick(void);
int    statistics_print(void);
void   pack_left_in_sys(void);

int      add_packet_to_queue(BitPack*);
BitPack* pop_packet_to_server(void);
BitPack* send_pack_to_arrive(PackGo*);
BitPack* get_pack_from_queue(void);
Queue*   find_queue_of_wfq(void);
Queue*   find_right_queue(void);

#endif // end __SIMULATE_H__
