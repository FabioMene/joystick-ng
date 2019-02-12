/*
 * jngd.h
 * 
 * Copyright 2018-2019 Fabio Meneghetti <fabiomene97@gmail.com>
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

#ifndef JNGD_H
#define JNGD_H 1

// Robe
#include <stdlib.h>
// File
#include <unistd.h>
// Socket UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
// altro
#include <pthread.h>
#include <errno.h>
// Constanti di jngd
#include "../libjngd-client/libjngd.h"


#ifdef DEBUG
    #include <stdio.h>
    
    #define printinit() 
    
    #define printi(fmt...) do{printf("[II] " fmt);putchar('\n');}while(0)
    #define printw(fmt...) do{printf("[WW] " fmt);putchar('\n');}while(0)
    #define printe(fmt...) do{printf("[EE] " fmt);putchar('\n');}while(0)
#else
    // Macro per syslog
    #include <syslog.h>
    
    #define printinit() do{close(0);close(1);close(2);openlog("jngd", LOG_CONS | LOG_PID, LOG_DAEMON);}while(0)
    
    #define printi(fmt...) syslog(LOG_INFO,    fmt);
    #define printw(fmt...) syslog(LOG_WARNING, fmt);
    #define printe(fmt...) syslog(LOG_ERR,     fmt);
#endif


//
#define JNGD_DATA_PATH   "/etc/jngd"
#define JNGD_GLOBAL_FILE "globals.def"
#define JNGD_GLOBAL_OPT  "globals"


//
#define MAX_THREADS 64


// Usato come argomento per i thread
typedef struct {
    int  fd;
    int* zero_on_exit;
} jngd_thread_arg_t;


// La routine che gestisce il client
extern void* client_service(void* arg);


// Usato per la creazione di thread e quando client_service esce
extern pthread_mutex_t threads_mutex;


// Ritorna -1 in caso di errore, 0 se l'utente non è privilegiato, 1 se lo è
int isUserElevated(int un_sock);


/// Prototipi gestore opzioni. Tutte le funzioni ritornano 0 in caso di successo

// Rappresentazione interna delle opzioni
typedef struct {
    jngd_option_type_e type;
    unsigned char      is_override; // Viene considerata solo nella risoluzione
    union {
        struct { // Per opzioni normali
            char name[256];
            char def_value[256];
            char value[256];
            char description[512];
        };
        char path[512]; // Per exec=
    };
} internal_option_t;

// Carica definizioni dei driver e globali, più le opzioni salvate
extern int drvoption_load();

// Carica il percorso eseguibile di driver in dst
extern int drvoption_read_exec(char* driver, char* dst);

// Duplica la lista di opzioni di driver (o, se NULL, quella globale) e la ritorna in dst
extern int drvoption_dump_list(char* driver, internal_option_t** dst, int* dstnum);

// Risolvi e leggi il valore di un opzione (nel formato driver.opzione o opzione).
extern int drvoption_resolve_option(char* option, char* dst);

// Scrivi un opzione (nel formato driver.opzione o opzione)
extern int drvoption_update_option(char* option, char* src);


/// Azioni. Se ritorna != 0 viene inviato solo il pacchetto di stato con quell'errore

// Controlla che la lunghezza del pacchetto sia almeno min
#define CHECK_SIZE(min) {int __min = (min); if(*len < __min){printw("Lunghezza pacchetto richiesta: %d, ricevuta: %d", __min, *len);return EINVAL;}}

int do_drv_launch(unsigned char* packet, int* len);

int do_drv_list(unsigned char* packet, int* len);


int do_drvopt_update(unsigned char* packet, int* len);

int do_drvopt_list(unsigned char* packet, int* len);

int do_drvopt_get(unsigned char* packet, int* len);

int do_drvopt_set(unsigned char* packet, int* len);


int do_js_soft_disconnect(unsigned char* packet, int* len);

int do_js_swap(unsigned char* packet, int* len);

#endif

