#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "../include/joystick-ng.h"

#define P(i) printf("%s: %ld\n", #i, sizeof(i))

typedef struct {
    unsigned int   num;
    
    // Non NULL se un driver lo sta gestendo (Questa è la connessione col driver)
    struct jng_connection_s* driver;
    
    jng_info_t        info;
    
    // Stato input
    jng_state_ex_t    state_ex;
    // RWLock sullo stato del joystick (client: reader, driver: writer)
    unsigned long          state_lock;
    // Coda e incrementale (per tenere traccia dei cambiamenti)
    unsigned long state_queue;
    unsigned int      state_inc;
    
    // Stato output
    jng_feedback_ex_t feedback_ex;
    // Spinlock perché un solo driver gestisce un joystick, ergo un solo reader per volta
    unsigned long        feedback_lock;
    // Coda e incrementale
    unsigned long feedback_queue;
    unsigned int      feedback_inc;
} jng_joystick_t;

// Dati connessione, valido per client e server
typedef struct jng_connection_s {
    // Il joystick
    jng_joystick_t* joystick;
    
    // Roba termporanea (gestione eventi)
    // Queste cache servono per tenere liberi gli spinlock
    struct {
        union {
            jng_state_ex_t    state_ex;
            jng_feedback_ex_t feedback_ex;
        };
        
        union {
            jng_event_t    event; // Per driver
            jng_event_ex_t event_ex; // Per client
        };
    } tmp;
    
    // Queste strutture servono per generare gli eventi (diff)
    union {
        jng_state_ex_t    state_ex;
        jng_feedback_ex_t feedback_ex;
    } diff; 
    
    // Modo lettura/scrittura
    unsigned int    mode;
    
    // Eventi da considerare
    unsigned int    evmask;
    
    // Buffer di eventi di lettura. Gli eventi in scrittura aggiornano la parte corrispondente
    unsigned long     rbuffer[4];
    // Valore da confrontare con quello del joystick, il che in teoria non crea race conditions
    // L'incremento di state_inc e feedback_inc avviene invece atomicamente
    unsigned int    r_inc;
} jng_connection_t;

int main(){
    P(jng_joystick_t);
    P(jng_connection_t);
    return 0;
}
