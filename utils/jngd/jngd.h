/*
 * jngd.h
 * 
 * Copyright 2018 Fabio Meneghetti <fabiomene97@gmail.com>
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

#include <syslog.h>
#include <pthread.h>

#include "../libjngd-client/libjngd.h"


#define printi(fmt...) syslog(LOG_INFO,    fmt);
#define printw(fmt...) syslog(LOG_WARNING, fmt);
#define printe(fmt...) syslog(LOG_ERR,     fmt);


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


/// Azioni. Se ritorna != 0 viene inviato solo il pacchetto di stato con quell'errore

// Controlla che la lunghezza del pacchetto sia almeno min
#define CHECK_SIZE(min) {int __min = (min); if(*len < __min){printw("Lunghezza pacchetto richiesta: %d, ricevuta: %d", __min, *len);return EINVAL;}}

int do_drv_launch(unsigned char* packet, int* len);

int do_drv_list(unsigned char* packet, int* len);


#endif
