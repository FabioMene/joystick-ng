/*
 * joystick-ng-core.h
 * 
 * Copyright 2015-2019 Fabio Meneghetti <fabiomene97@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

// Nel modulo kernel includere SOLO QUESTO HEADER, che include anche joystick-ng.h

#ifndef JOYSTICK_NG_CORE_H
#define JOYSTICK_NG_CORE_H 1

// DEBUG v
#define JNG_LOG_TAG "[joystick-ng]"

#define printi(f, a...) printk(KERN_INFO  JNG_LOG_TAG "[Info] " f "\n", ##a)
#define printw(f, a...) printk(KERN_ALERT JNG_LOG_TAG "[Warn] " f "\n", ##a)
#define printe(f, a...) printk(KERN_ERR   JNG_LOG_TAG "[Err ] " f "\n", ##a)
// DEBUG ^

// Il nome dei due device e la classe sysfs. SUBSYSTEM in udev
#define JNG_DRIVER_NAME "joystick-ng"

// Il nome dei tre device per controllo, driver e client, KERNEL in udev (penso)
#define JNG_CONTROL_DEVICE_NAME "control"
#define JNG_DRIVERS_DEVICE_NAME "driver"
#define JNG_CLIENTS_DEVICE_NAME "device"

// Il numero di joystick supportati (32 penso che sia abbastanza)
#define JNG_TOT 32

// Quanti eventi per connessione tenere al massimo
#define JNG_MAX_CONN_EV 256

// Roba interna

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>
#include "../include/joystick-ng.h"
#include "list.h"

// Per la dipendenza ciclica
struct jng_connection_s;

// Rappresenta un joystick, NON necessariamente connesso
typedef struct {
    uint32_t num;
    
    // Non NULL se un driver lo sta gestendo (Questa è la connessione col driver)
    struct jng_connection_s* driver;
    
    jng_info_t info;
    
    // Stato input
    jng_state_ex_t state_ex;
    // RWLock sullo stato del joystick (client: reader, driver: writer)
    rwlock_t state_lock;
    // Coda e incrementale (per tenere traccia dei cambiamenti)
    wait_queue_head_t state_queue;
    uint32_t          state_inc;
    
    // Stato output
    jng_feedback_ex_t feedback_ex;
    // Spinlock perché un solo driver gestisce un joystick, ergo un solo reader per volta
    spinlock_t feedback_lock;
    // Coda e incrementale
    wait_queue_head_t feedback_queue;
    uint32_t          feedback_inc;
} jng_joystick_t;


/// Membri di jng_connection_t specifici per client

// Elemento della lista cl_aggregate, usato dai client in mod. aggregata
typedef struct {
    jng_joystick_t* js;   // Joystick associato
    jng_state_ex_t  diff; // Differenze per questo joystick
    uint32_t        inc;  // Incrementale
} jng_aggregate_element_t;


/// Membri di jng_connection_t specifici per driver

// Nessuno per ora


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
    uint32_t mode;
    
    // Eventi da considerare
    uint32_t evmask;
    
    // Buffer di eventi di lettura. Gli eventi in scrittura aggiornano la parte corrispondente
    jng_list_t rbuffer;
    // Valore da confrontare con quello del joystick, il che in teoria non crea race conditions
    // L'incremento di state_inc e feedback_inc avviene invece atomicamente
    uint32_t r_inc;
    
    union {
        struct {
            // Lista dei joystick in modalità aggregata
            jng_list_t aggregate;
        } client_data;
        
        struct {
            // L'incrementale dell'ultima volta che è stato generato l'evento JNG_CTRL_SOFT_DISCONNECT
            int32_t soft_connected_inc;
        } driver_data;
    };
} jng_connection_t;

// Scorciatoie tattica, da usare in joystick-ng-{driver,client}-fops.c dove struct file* fp è definito
#define jng_connection_data ((jng_connection_t*)fp->private_data)


// Funzioni (ex macro) per bloccare/sbloccare gli spinlock

static __always_inline void jng_state_rlock(jng_joystick_t* js){
    read_lock(&js->state_lock);
}

static __always_inline void jng_state_runlock(jng_joystick_t* js){
    read_unlock(&js->state_lock);
}

static __always_inline void jng_state_wlock(jng_joystick_t* js){
    write_lock(&js->state_lock);
}

static __always_inline void jng_state_wunlock(jng_joystick_t* js){
    write_unlock(&js->state_lock);
}


static __always_inline void jng_feedback_lock(jng_joystick_t* js){
    spin_lock(&js->feedback_lock);
}

static __always_inline void jng_feedback_unlock(jng_joystick_t* js){
    spin_unlock(&js->feedback_lock);
}


// Variabili globali esterne

// Operazioni sui file, variano da client a driver
extern struct file_operations joystick_ng_control_fops;
extern struct file_operations joystick_ng_driver_fops;
extern struct file_operations joystick_ng_client_fops;

// I joystick
extern jng_joystick_t jng_joysticks[JNG_TOT];
extern spinlock_t     jng_joysticks_lock;

// Client e driver bloccano in lettura, le operazioni di controllo bloccano in scrittura
// Questo è necessario quando si usa jng_connection_data->joystick
extern rwlock_t       jng_control_lock;

// Relative funzioni
static __always_inline void jng_control_rlock(void){
    read_lock(&jng_control_lock);
}

static __always_inline void jng_control_runlock(void){
    read_unlock(&jng_control_lock);
}

static __always_inline void jng_control_wlock(void){
    write_lock(&jng_control_lock);
}

static __always_inline void jng_control_wunlock(void){
    write_unlock(&jng_control_lock);
}

#endif

