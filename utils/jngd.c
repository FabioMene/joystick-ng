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

#define printi(fmt...) syslog(LOG_INFO,    fmt);
#define printw(fmt...) syslog(LOG_WARNING, fmt);
#define printe(fmt...) syslog(LOG_ERR,     fmt);

unsigned char buffer[32768];

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
    
    openlog("jngd", LOG_CONS | LOG_PID, LOG_DAEMON);
    printi("Inizializzato");
    
    // Inizializza socket
    int sockfd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
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
      _skip_to_recv:
        int len = recvfrom(sockfd, buffer, 32768, 0, NULL, NULL);
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
                packet[2 + packet[0] - 1 + packet[1] - 1] = 0;
                
                int i, n;
                
                // Per caricare il percorso dell'eseguibile
                jngdsett_load(packet + 2);
                char exec_path[512];
                jngdsett_read("exec", exec_path);
                jngdsett_reset();
                
                // Creazione vettore argomenti
                char** new_argv = NULL;
                
                char* last_start = packet + 2 + packet[0];
                char* cursor = last_start
                
                for(i = 0, n = 0;i < packet[1];i++){
                    
                }
                
                
                } break;
        }
        
        
        if(len < 3){
            printw("Pacchetto malformato (non presenta l'header), scartato");
            continue;
        }
        unsigned char eplen = (unsigned char)buffer[0];
        unsigned char pplen = (unsigned char)buffer[1];
        unsigned char arnum = (unsigned char)buffer[2];
        if(len < (3 + eplen + pplen + arnum)){ // arnum conta solo gli \0 degli argomenti, non è accurato
            printw("Pacchetto malformato (la lunghezza è inferiore a quella dichiarata), scartato");
            continue;
        }
        char* executable_path = buffer + 3;
        char  prefs_var[268] = "JNG_DRIVER=";
        strncat(prefs_var, buffer + 3 + eplen + 1, pplen);
        
        char** new_argv = malloc(sizeof(char*) * (arnum + 2));
        new_argv[0] = executable_path;
        
        int i   = 3 + eplen + 1 + pplen + 1;
        int arg = 1;
        while(arnum && i < 32767){
            new_argv[arg++] = buffer + i;
            if(!(--arnum)) break;
            while(i < 32767 && buffer[i] != 0) i++;
            i++;
        }
        new_argv[arg] = NULL;
        
        char* new_envp[] = { prefs_var, NULL };
        
        // Crea un nuovo processo
        printi("Avvio %s...", executable_path);
        
        pid = fork();
        if(pid < 0){
            printe("Impossibile eseguire fork()");
        } else if(pid == 0){
            execve(executable_path, new_argv, new_envp);
            printe("Errore execve()");
            return 0;
        }
    }
    
    return 0;
}

