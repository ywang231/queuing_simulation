/*
 * File:     datadef.h
 * Author:   Eric Wang
 * Date:     May 25, 2024
 * Description: Define the data structure used in this simulation.
 */


#ifndef __DATADEF_H__
#define __DATADEF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Simulation parameters - read from the config file
#define __CONFIG_FILE__    "config.ini"
#define __FREE__(ptr)      do { if(ptr != NULL) { free(ptr); ptr = NULL; } } while(0)
#define __CLOSE_FILE__(file_ptr) do { if(file_ptr != NULL) { fclose(file_ptr); file_ptr = NULL; } } while(0)
#define __RMODE_FILE_OPEN__(file_ptr, file_name) \
    do { \
        (file_ptr) = fopen((file_name), "r"); \
        if (!(file_ptr)) { \
            fprintf(stderr, "Failed to open file '%s' for reading\n", (file_name)); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

#define __MALLOC__(ptr, size) \
    do { \
        (ptr) = malloc((size)); \
        if (!(ptr)) { \
            fprintf(stderr, "Memory allocation failed\n"); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

#define __POSIIVE_INFINITE__ 1.0e+30

typedef unsigned long  num_t;
typedef long double lfnum_t;
typedef double fnum_t;
typedef int snum_t;

typedef enum {FIFO = 0, SPQ, WFQ} QUEUE_MODE;
// Define the source type
typedef enum { AUDIO = 0, VIDEO, DATA, ALLTYPE} SOURCE_TYPE;

// Packet
struct Packet;
typedef struct Packet Packet;
struct Packet {
    // Packet type - 3 types of packets: audio, video, and data
    SOURCE_TYPE type;
    // Packet size
    fnum_t size_kb;
    // Packet arrival time
    fnum_t arrival_time;
    // Packet departure time
    fnum_t depart_time;
    // Packet start serving time
    fnum_t serve_start_time;
    // Used to link the next packet in the queue
    Packet* next;
    // Used to check if the packet is dropped
    bool is_dropped;
};

// Define the event type
typedef enum { ARRIVAL = 1, DEPART, UNKNOWN } EVENT_TYPE;
struct Event;
typedef struct Event Event;
struct Event {
    // Event type
    EVENT_TYPE type;
    // Event time
    fnum_t event_time;
    // Data reference involved in the event
    void* data_related;
};

// Queue of the system used to manage the packets in the queue.
struct Queue; //FIFO
typedef struct Queue Queue;
typedef bool (*Append)(Queue*, Packet*);
typedef Packet* (*Pop)(Queue*);
struct Queue {
    SOURCE_TYPE type;
    Packet* head;
    fnum_t size_kb;
    fnum_t max_size_kb;
    Append append;
    Pop pop;
};
// Append a packet to the tail of the queue
bool append(Queue*, Packet*);
// Pop a packet from the head of the queue
Packet* pop(Queue*);


// Server
enum SEVER_STATUS {IDLE = 0, BUSY, STATUS_NUM};
struct Server;
typedef struct Server Server;
struct Server {
   enum SEVER_STATUS status;
   Packet* packet_in_process;
   int serve_speed;
};

/****************************************************************************
 Generator used to generate the three types of packets according to the given 
 bps and ontime.
****************************************************************************/
struct BitEmitter;
typedef struct BitEmitter BitEmitter;
typedef Packet* (*EmitterPop)(BitEmitter*);
typedef void (*EmitterTick)(BitEmitter*);

// Packet generator
struct BitEmitter {
    SOURCE_TYPE type; //Type
    snum_t concur; // number of packet generated at the same time
    fnum_t mean_on; //mean on_time
    fnum_t mean_off; //mean off_time
    num_t bps; // Rate of generating
    fnum_t packet_size_kb; // Unit - kb
    
    fnum_t on_time_start;
    fnum_t on_time; // Follow the exponential distribution
    fnum_t off_time; // Follow the exponential distribution
    num_t arrival_num;
    fnum_t arraval_time;
    num_t total_sources;
    EmitterPop pop;
    EmitterTick tick;
};

void emitter_tick(BitEmitter*);
Packet* emitter_pop(BitEmitter*);

// Data collector
typedef struct  {
    num_t class_arrived[ALLTYPE];
    fnum_t sim_time;
    num_t class_served[ALLTYPE]; // number of packets served
    lfnum_t class_served_kb[ALLTYPE];  // size of packets served in kb
    lfnum_t total_served_kb; // Total size of the packets served in kb
    num_t class_delayed[ALLTYPE];
    num_t class_dropped[ALLTYPE];
    num_t class_remained[ALLTYPE];
    snum_t output_interval;
    fnum_t max_sim_time;
} Sim;

#ifdef __cplusplus
}
#endif

#endif // __DATATYPE_H__
