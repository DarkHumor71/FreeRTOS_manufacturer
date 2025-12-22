#ifndef PETRI_NET_H
#define PETRI_NET_H

#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"

#if defined(_MSC_VER)
 // MSVC does not support C11 atomics in C mode
typedef volatile long atomic_bool;
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_load(ptr) (*(ptr))
#else
#include <stdatomic.h>
#endif

#define MAX_PLACES 15
#define MAX_TRANSITIONS 20

typedef struct {
    int tokens;
    char name[32];
    SemaphoreHandle_t mutex;
} Place;

typedef struct {
    int input_places[5];
    int output_places[5];
    int input_weights[5];
    int output_weights[5];
    char name[32];
    bool enabled;
} Transition;

typedef struct {
    Place places[MAX_PLACES];
    Transition transitions[MAX_TRANSITIONS];
    int num_places;
    int num_transitions;
    SemaphoreHandle_t net_mutex;
} PetriNet;

extern PetriNet manufacturing_net;

void init_petri_net(void);
int add_place(const char* name, int initial_tokens);
int add_transition(const char* name);
void add_arc_input(int trans_idx, int place_idx, int weight);
void add_arc_output(int trans_idx, int place_idx, int weight);
bool is_transition_enabled(int trans_idx);
bool fire_transition(int trans_idx);
int get_place_tokens(int place_idx);

#endif // PETRI_NET_H
