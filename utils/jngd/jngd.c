/*
 * jngd.c
 * 
 * Copyright 2017-2018 Fabio Meneghetti <fabiomene97@gmail.com>
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

// joystick-ng daemon
// Questa utilità fornisce un modo unificato per gestire le preferenze e lancio dei driver,
// e per controllare il modulo kernel
// Questo programma avvia un server unix locale. La comunicazione avviene tramite la
// libreria libjngd-client (vedi jngctl per un implementazione)


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <errno.h>

#include <signal.h>

// Socket UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

// Operazioni file
#include <unistd.h>

// Costanti E*
#include <errno.h>

#include "jngd.h"

unsigned char buffer[32768];

int servfd = -1;

void sigterm_handler(int unused){
    close(servfd);
    unlink(SOCKET_FILE);
    printw("Uscita causata da SIGTERM");
    exit(0);
}

pthread_mutex_t threads_mutex = PTHREAD_MUTEX_INITIALIZER;
int             threads_used[MAX_THREADS];
pthread_t       threads[MAX_THREADS];

int main(void){
    // Inizializzazione
    chdir("/");
    
    close(0);
    close(1);
    close(2);
    
    
    // Ignora l'handling dei processi figli (non diventeranno zombie)
    signal(SIGCHLD, SIG_IGN);
    
    // In caso jngd venga terminato (da systemd per esempio, service joystick-ng stop)
    signal(SIGTERM, sigterm_handler);
    
    
    openlog("jngd", LOG_CONS | LOG_PID, LOG_DAEMON);
    printi("Inizializzazione");
    
    
    // Inizializza socket server
    servfd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if(servfd < 0){
        printe("Impossibile inizializzare il socket");
        return 1;
    }
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILE);
    unlink(sa.sun_path);
    if(bind(servfd, (struct sockaddr*)&sa, sizeof(sa.sun_family) + strlen(SOCKET_FILE) + 1) < 0){
        printe("bind() fallito");
        return 1;
    }
    
    if(listen(servfd, 5) < 0){
        printe("listen() fallito");
        return 1;
    }
    
    printi("Pronto");
    
    memset(threads_used, 0, sizeof(threads_used));
    
    while(1){
        int client = accept4(servfd, NULL, NULL, SOCK_CLOEXEC);
        
        if(client < 0){
            printe("Errore accept()");
            return 1;
        }
        
        pthread_mutex_lock(&threads_mutex);
            // Cerca uno slot libero
            int i;
            for(i = 0;i < MAX_THREADS;i++){
                if(threads_used[i] == 0){
                    jngd_thread_arg_t* arg = malloc(sizeof(jngd_thread_arg_t));
                    if(arg == NULL){
                        printw("Non c'è abbastanza memoria");
                        break;
                    }
                    
                    arg->fd           = client,
                    arg->zero_on_exit = threads_used + i;
                    
                    if(pthread_create(threads + i, NULL, client_service, arg)){
                        free(arg);
                        printw("Impossibile avviare il thread per il socket %d", client);
                        break;
                    }
                    
                    threads_used[i] = 1;
                    break;
                }
            }
            
            if(i == MAX_THREADS){
                printw("Troppi thread");
                close(client);
            }
        pthread_mutex_unlock(&threads_mutex);
    }
    
    return 0;
}
