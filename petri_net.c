#include "petri_net.h"
#include <stdio.h>
#include <stdlib.h>

PetriNet manufacturing_net;

extern atomic_bool status_dirty;

void init_petri_net(void) {
    manufacturing_net.num_places = 0;
    manufacturing_net.num_transitions = 0;
    manufacturing_net.net_mutex = xSemaphoreCreateMutex();

    if (manufacturing_net.net_mutex == NULL) {
        printf("ERROR: Failed to create net mutex\n");
        exit(1);
    }

    for (int i = 0; i < MAX_PLACES; i++) {
        manufacturing_net.places[i].tokens = 0;
        manufacturing_net.places[i].mutex = xSemaphoreCreateMutex();

        if (manufacturing_net.places[i].mutex == NULL) {
            printf("ERROR: Failed to create place mutex %d\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < MAX_TRANSITIONS; i++) {
        for (int j = 0; j < 5; j++) {
            manufacturing_net.transitions[i].input_places[j] = -1;
            manufacturing_net.transitions[i].output_places[j] = -1;
            manufacturing_net.transitions[i].input_weights[j] = 0;
            manufacturing_net.transitions[i].output_weights[j] = 0;
        }
        manufacturing_net.transitions[i].enabled = false;
    }
}

int add_place(const char* name, int initial_tokens) {
    if (manufacturing_net.num_places >= MAX_PLACES) {
        printf("ERROR: Cannot add place '%s' - max places reached\n", name);
        return -1;
    }

    int idx = manufacturing_net.num_places++;
    snprintf(manufacturing_net.places[idx].name, 32, "%s", name);
    manufacturing_net.places[idx].tokens = initial_tokens;
    return idx;
}

int add_transition(const char* name) {
    if (manufacturing_net.num_transitions >= MAX_TRANSITIONS) {
        printf("ERROR: Cannot add transition '%s' - max transitions reached\n", name);
        return -1;
    }

    int idx = manufacturing_net.num_transitions++;
    snprintf(manufacturing_net.transitions[idx].name, 32, "%s", name);
    return idx;
}

void add_arc_input(int trans_idx, int place_idx, int weight) {
    for (int i = 0; i < 5; i++) {
        if (manufacturing_net.transitions[trans_idx].input_places[i] == -1) {
            manufacturing_net.transitions[trans_idx].input_places[i] = place_idx;
            manufacturing_net.transitions[trans_idx].input_weights[i] = weight;
            break;
        }
    }
}

void add_arc_output(int trans_idx, int place_idx, int weight) {
    for (int i = 0; i < 5; i++) {
        if (manufacturing_net.transitions[trans_idx].output_places[i] == -1) {
            manufacturing_net.transitions[trans_idx].output_places[i] = place_idx;
            manufacturing_net.transitions[trans_idx].output_weights[i] = weight;
            break;
        }
    }
}

bool is_transition_enabled(int trans_idx) {
    Transition* t = &manufacturing_net.transitions[trans_idx];

    for (int i = 0; i < 5; i++) {
        if (t->input_places[i] == -1) break;

        int place_idx = t->input_places[i];
        int required = t->input_weights[i];

        xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
        int available = manufacturing_net.places[place_idx].tokens;
        xSemaphoreGive(manufacturing_net.places[place_idx].mutex);

        if (available < required) {
            return false;
        }
    }
    return true;
}

bool fire_transition(int trans_idx) {
    Transition* t = &manufacturing_net.transitions[trans_idx];

    xSemaphoreTake(manufacturing_net.net_mutex, portMAX_DELAY);

    if (!is_transition_enabled(trans_idx)) {
        xSemaphoreGive(manufacturing_net.net_mutex);
        return false;
    }

    for (int i = 0; i < 5; i++) {
        if (t->input_places[i] == -1) break;

        int place_idx = t->input_places[i];
        int weight = t->input_weights[i];

        xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
        manufacturing_net.places[place_idx].tokens -= weight;
        xSemaphoreGive(manufacturing_net.places[place_idx].mutex);
    }

    for (int i = 0; i < 5; i++) {
        if (t->output_places[i] == -1) break;

        int place_idx = t->output_places[i];
        int weight = t->output_weights[i];

        xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
        manufacturing_net.places[place_idx].tokens += weight;
        xSemaphoreGive(manufacturing_net.places[place_idx].mutex);
    }

    xSemaphoreGive(manufacturing_net.net_mutex);

    // Mark status as dirty for immediate update
    atomic_store(&status_dirty, true);

    return true;
}

int get_place_tokens(int place_idx) {
    xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
    int tokens = manufacturing_net.places[place_idx].tokens;
    xSemaphoreGive(manufacturing_net.places[place_idx].mutex);
    return tokens;
}
