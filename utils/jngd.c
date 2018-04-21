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
// Questa utilità aiuta la programmazione di driver, in quanto fornisce
// un modo unificato per gestire le preferenze e il lancio dei driver stessi
// Tramite la libreria libjngdsett.so si può accedere alle preferenze
// Per lanciare un driver si usa il comando jngctl (vedi jngctl.c), utile con udev (RUN+=)

// La comunicazione tra jngctl e jngd avviene tramite socket unix
// Il socket è /var/run/jngdriverd.socket

// Il primo byte del pacchetto indica l'azione, da cui dipende la struttura del resto del pacchetto

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <grp.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "jngd.h"
#include "libjngdsett/libjngdsett.h"

#define printi(fmt...) syslog(LOG_INFO,    fmt);
#define printw(fmt...) syslog(LOG_WARNING, fmt);
#define printe(fmt...) syslog(LOG_ERR,     fmt);

unsigned char buffer[32768];

int sockfd = -1;

void sigterm_handler(int sig){
    close(sockfd);
    unlink(SOCKET_FILE);
    printw("Uscita causata da SIGTERM");
    exit(0);
}

int main(int argc, char* argv[]){
    // Demone standard
    int pid = fork();
    if(pid < 0) return 1;
    if(pid)     return 0;
    
    int sid = setsid();
    if(sid < 0) return 1;
    chdir("/");
    
    close(0);
    close(1);
    close(2);
    
    // Ignora l'handling dei processi figli (non diventeranno zombie)
    signal(SIGCHLD, SIG_IGN);
    
    // In caso jngd venga terminato (da systemd per esempio, service joystick-ng stop)
    signal(SIGTERM, sigterm_handler);
    
    openlog("jngd", LOG_CONS | LOG_PID, LOG_DAEMON);
    printi("Inizializzato");
    
    // Inizializza socket
    sockfd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(sockfd < 0){
        printe("Impossibile inizializzare il socket");
        return 1;
    }
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILE);
    unlink(sa.sun_path);
    if(bind(sockfd, (struct sockaddr*)&sa, sizeof(sa.sun_family) + strlen(SOCKET_FILE) + 1) < 0){
        printe("bind() fallito");
        close(sockfd);
        return 1;
    }
    
    // Imposta i permessi del socket
    struct group* grp = getgrnam(SOCKET_GROUP);
    if(!grp){
        printw("Impossibile ottenere informazioni sul gruppo \"%s\"", SOCKET_GROUP);
    } else {
        // chown root:SOCKET_GROUP
        chown(SOCKET_FILE, 0, grp->gr_gid);
        chmod(SOCKET_FILE, 0660);
    }
    
    unsigned char* packet = buffer + 1;
    
    // Mainloop
    while(1){
        int len;
        
      _skip_to_recv:
        len = recvfrom(sockfd, buffer, 32768, 0, NULL, NULL);
        if(len < 0){
            printe("Errore ricezione pacchetto (errno %d)", errno);
            close(sockfd);
            unlink(SOCKET_FILE);
            return 1;
        }
        
        if(len < 1){
            printw("Pacchetto senza header (ed è di un solo byte)");
            continue;
        }
        
        len--;
        
        #define check_size(min) {int __min = (min); if(len < __min){printw("Lunghezza pacchetto richiesta: %d, ricevuta: %d", __min, len);goto _skip_to_recv;}}
        
        switch(buffer[0]){
            case ACTION_DRV_LAUNCH: {
                // Controlli pacchetti
                check_size(2);
                check_size(2 + packet[0] + packet[1]);
                
                // Sanity check
                packet[2 + packet[0] - 1] = 0;
                packet[2 + packet[0] + packet[1] - 1] = 0;
                
                int i, n;
                
                // Per caricare il percorso dell'eseguibile
                if(jngdsett_load((char*)packet + 2) < 0){
                    printe("[DRV_LAUNCH] Il driver \"%s\" non esiste", packet + 2);
                    goto _skip_to_recv;
                }
                
                char exec_path[512];
                i = jngdsett_read("exec", exec_path);
                
                jngdsett_reset();
                
                if(i){
                    printe("[DRV_LAUNCH] Il driver %s non ha exec", packet + 2);
                    goto _skip_to_recv;
                }
                
                // Creazione vettore argomenti
                int argc = 2; // Minimo (eseguibile, NULL)
                for(i = 2 + packet[0];i < len;i++){
                    if(packet[i] == 0) argc++;
                }
                
                char** new_argv = malloc(argc * sizeof(char*));
                if(!new_argv){
                    printe("[DRV_LAUNCH] Memoria esaurita");
                    goto _skip_to_recv;
                }
                
                new_argv[0] = exec_path;
                
                char* last = (char*)packet + 2 + packet[0];
                for(i = 2 + packet[0], n = 1;i < len;i++){
                    if(packet[i] == 0){
                        new_argv[n++] = last;
                        last = (char*)packet + i + 1;
                    }
                }
                
                new_argv[n] = NULL;
                
                // Creazione vettore variabili ambientali
                char jng_driver_var[268] = "JNG_DRIVER=";
                strcat(jng_driver_var, (char*)packet + 2);
                
                char* new_envp[] = {
                    jng_driver_var, NULL
                };
        
                // Esecuzione execve
                printi("[DRV_LAUNCH] drv %s (%s)", packet + 2, exec_path);
        
                pid_t pid = fork();
                if(pid < 0){
                    printe("Impossibile eseguire fork()");
                } else if(pid == 0){
                    execve(exec_path, new_argv, new_envp);
                    printe("Errore execve()");
                    _Exit(1);
                }
                
                free(new_argv);
            } break;
            
            case ACTION_DRV_MODSETT: {
                // Controllo pacchetti e sanity check
                check_size(3);
                check_size(3 + packet[0] + packet[1] + packet[2]);
                
                packet[3 + packet[0] - 1] = 0;
                packet[3 + packet[0] + packet[1] - 1] = 0;
                packet[3 + packet[0] + packet[1] + packet[2] - 1] = 0;
                
                // Carica il driver
                if(jngdsett_load((char*)packet + 3)){
                    printe("[DRV_MODSETT] Driver \"%s\" non trovato", packet + 3);
                    goto _skip_to_recv;
                }
                
                int res = jngdsett_write((char*)packet + 3 + packet[0], (char*)packet + 3 + packet[0] + packet[1]);
                
                jngdsett_reset();
                
                if(res){
                    printe("[DRV_MODSETT] Scrittura fallita per il driver %s", packet + 3);
                }
            } break;
        }
        
        #undef check_size
    }
    
    return 0;
}

