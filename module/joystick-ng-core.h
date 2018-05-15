/*
 * joystick-ng-core.h
 * 
 * Copyright 2015-2017 Fabio Meneghetti <fabiomene97@gmail.com>
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

// Il nome dei due device per drivers e devices, KERNEL in udev
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
#include "queue.h"

// Per la dipendenza ciclica
struct jng_connection_s;

// Rappresenta un joystick, NON necessariamente connesso
typedef struct {
    unsigned int   num;
    
    // Non NULL se un driver lo sta gestendo (Questa è la connessione col driver)
    struct jng_connection_s* driver;
    
    jng_state_t    state;
    jng_feedback_t feedback;
    jng_info_t     info;
    
    // RWLock sullo stato del joystick (client: reader, driver: writer)
    rwlock_t       state_lock;
    // Spinlock perché un solo driver gestisce un joystick, ergo un solo reader per volta
    spinlock_t     feedback_lock;
    
    // Code di lettura, per gli eventi. i valori incrementali incrementano ad ogni cambiamento
    // e non dipendono dal driver/client
    wait_queue_head_t state_queue;
    unsigned int      state_inc;
    
    wait_queue_head_t feedback_queue;
    unsigned int      feedback_inc;
    
} jng_joystick_t;

// Dati connessione, valido per client e server
typedef struct jng_connection_s {
    // Il joystick
    jng_joystick_t* joystick;
    
    // Roba termporanea (gestione eventi)
    // Queste cache servono per tenere liberi gli spinlock
    union {
        jng_state_t     tmpstate;
        jng_feedback_t  tmpfeedback;
    };
    jng_event_t     tmpevent;
    
    // Queste strutture servono per generare gli eventi (diff)
    union {
        jng_state_t    state;
        jng_feedback_t feedback;
    } diff; 
    
    // Modo lettura/scrittura
    unsigned int    mode;
    
    // Eventi da considerare
    unsigned int    evmask;
    
    // Buffer di eventi di lettura. Gli eventi in scrittura aggiornano la parte corrispondente
    jng_queue_t     rbuffer;
    // Valore da confrontare con quello del joystick, il che in teoria non crea race conditions
    // L'incremento di state_inc e feedback_inc avviene invece atomicamente
    unsigned int    r_inc;
} jng_connection_t;

// Scorciatoie tattica, da usare in joystick-ng-{driver,client}-fops.c dove struct file* fp è definito
#define jng_connection_data    ((jng_connection_t*)fp->private_data)

#define jng_state_rlock(conn)     read_lock(&conn->joystick->state_lock);
#define jng_state_runlock(conn)   read_unlock(&conn->joystick->state_lock);
#define jng_state_wlock(conn)     write_lock(&conn->joystick->state_lock);
#define jng_state_wunlock(conn)   write_unlock(&conn->joystick->state_lock);

#define jng_feedback_lock(conn)   spin_lock(&conn->joystick->feedback_lock);
#define jng_feedback_unlock(conn) spin_unlock(&conn->joystick->feedback_lock);

// Variabili globali esterne

// Operazioni sui file, variano da client a driver
extern struct file_operations joystick_ng_control_fops;
extern struct file_operations joystick_ng_driver_fops;
extern struct file_operations joystick_ng_client_fops;

// I joystick
extern jng_joystick_t jng_joysticks[JNG_TOT];
extern spinlock_t     jng_joysticks_lock;


// Aggiunge un evento sulla coda di lettura
static inline void jng_add_event(jng_connection_t* conn, unsigned short type, unsigned int what, int val){
    conn->tmpevent.type  = type;
    conn->tmpevent.what  = what;
    conn->tmpevent.value = val;
    jng_queue_add(&conn->rbuffer, &conn->tmpevent);
}

#endif

