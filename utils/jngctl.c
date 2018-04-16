/*
 * jngdctl.c
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

// jngctl
// Questo programma permette di interfacciarsi con jngd

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>

#include "jngd.h"
#include "libjngdsett/libjngdsett.h"


void usage(char* fmt, ...){
    printf("jngctl, interfaccia di controllo per jngd\n"
           "Comandi disponibili:\n"
           "  jngctl dlist\n"
           "    Stampa una lista dei driver installati\n"
           "\n"
           "  jngctl drv <driver> exec [argomenti...]\n"
           "    Avvia un driver (In genere usato con udev)\n"
           "\n"
           "  jngctl drv <driver> opt list\n"
           "    Stampa le opzioni del driver e la loro descrizione\n"
           "\n"
           "  jngctl drv <driver> opt get [opzione]\n"
           "    Stampa il valore dell'opzione. Se non è specificata stampa tutti i valori\n"
           "\n"
           "  jngctl drv <driver> opt set <opzione> <val>\n"
           "    Imposta l'opzione\n"
           "\n"
           "  jngctl help\n"
           "    Questo aiuto\n");
    if(fmt){
        printf("Errore: ");
        va_list l;
        va_start(l, fmt);
        vprintf(fmt, l);
        va_end(l);
        printf("\n");
        exit(1);
    }
    exit(0);
}

int jngd_fd = -1;
struct sockaddr_un jngd_addr;

void jngd_connect(){
    jngd_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(jngd_fd < 0){
        printf("Impossibile creare il socket locale\n");
        exit(1);
    }
    
    jngd_addr.sun_family = AF_UNIX;
    strcpy(jngd_addr.sun_path, SOCKET_FILE);
}

void jngd_send(unsigned char* buffer, int len){
    if(sendto(jngd_fd, buffer, len, 0, (struct sockaddr*)&jngd_addr, sizeof(jngd_addr.sun_family) + strlen(jngd_addr.sun_path) + 1) < 0){
        printf("Impossibile inviare al socket locale.\nAssicurarsi che jngd sia attivo e di avere i permessi adeguati\n");
        exit(1);
    }
}

int main(int argc, char* argv[]){
    if(argc < 2) usage(NULL);
    
    int i;
    
    if(strcmp(argv[1], "dlist") == 0){
        DIR* dir = opendir("/etc/jngd/defs/");
        if(!dir){
            printf("Impossibile aprire la directory /etc/jngd/defs\n");
            return 1;
        }
        printf("Driver installati:\n");
        struct dirent* de;
        while((de = readdir(dir)) != NULL){
            if(strncmp(de->d_name, ".", 1)) printf("%s\n", de->d_name);
        }
        closedir(dir);
        return 1;
    }
    
    if(strcmp(argv[1], "drv") == 0){
        if(argc < 4) usage("Il comando drv richiede almeno due argomenti");
        
        char* driver = argv[2];
        
        if(strcmp(argv[3], "exec") == 0){
            unsigned char buffer[32768];
            char* packet = (char*)buffer + 1;
            
            buffer[0] = ACTION_DRV_LAUNCH;
            
            packet[0] = (unsigned char)strlen(driver) + 1;
            strcpy(packet + 2, driver);
            
            int arg_start = 2 + packet[0];
            for(i = 4;i < argc;i++){
                strcpy(packet + arg_start, argv[i]);
                arg_start += strlen(argv[i]) + 1;
            }
            
            packet[1] = arg_start - 2 - buffer[0];
            
            jngd_connect();
            jngd_send(buffer, 1+ 2 + packet[0] + packet[1]);
            return 0;
        }
        
        if(strcmp(argv[3], "opt") == 0){
            if(argc < 5) usage("Il comando drv:opt richiede almeno un argomento");
            
            if(jngdsett_load_onlydef(driver)){
                printf("Impossibile caricare la definizione del driver\n"
                       "Il driver potrebbe non essere installato\n");
                return 1;
            }
            
            if(strcmp(argv[4], "list") == 0){
                int num;
                jngdsett_opt_t* options = jngdsett_optdata(&num);
                
                char* types[] = {
                    "int",
                    "double",
                    "string"
                };
                
                for(i = 0;i < num;i++){
                    if(options[i].type == JNGDSETT_TYPE_EXEC){
                        printf("Percorso eseguibile\n    %s\n\n", options[i].value);
                    } else {
                        printf("%s (%s)\n    %s\n    [%s]\n\n", options[i].name, types[options[i].type], options[i].description, options[i].value);
                    }
                }
                return 0;
            }
            
            if(strcmp(argv[4], "get") == 0){
                jngdsett_load(driver);
                
                int num;
                jngdsett_opt_t* options = jngdsett_optdata(&num);
                
                char* match = NULL;
                
                if(argc >= 6){
                    match = argv[5];
                }
                
                for(i = 0;i < num;i++){
                    if(!match || strcmp(options[i].name, match) == 0){
                        if(!match) printf("%s: ", options[i].name);
                        
                        switch(options[i].type){
                            case JNGDSETT_TYPE_INT: {
                                int val;
                                sscanf(options[i].value, "%i", &val);
                                printf("%d\n", val);
                                } break;
                            
                            case JNGDSETT_TYPE_DOUBLE: {
                                double val;
                                sscanf(options[i].value, "%lf", &val);
                                printf("%lf\n", val);
                                } break;
                            
                            case JNGDSETT_TYPE_STRING:
                            case JNGDSETT_TYPE_EXEC:
                                printf("%s\n", options[i].value);
                                break;
                        }
                    }
                }
                return 0;
            }
            
            if(strcmp(argv[4], "set") == 0){
                if(argc < 7) usage("drv:opt:set richiede due argomenti");
                
                unsigned char buffer[32768];
                char* packet = (char*)buffer + 1;
                
                buffer[0] = ACTION_DRV_MODSETT;
                
                packet[0] = (unsigned char)strlen(driver) + 1;
                strcpy(packet + 3, driver);
                
                packet[1] = (unsigned char)strlen(argv[5]) + 1;
                strcpy(packet + 3 + packet[0], argv[5]);
                
                packet[2] = (unsigned char)strlen(argv[6]) + 1;
                strcpy(packet + 3 + packet[0] + packet[1], argv[6]);
                
                jngd_connect();
                jngd_send(buffer, 1+ 3 + packet[0] + packet[1] + packet[2]);
                return 0;
            }
            
            usage("Comando drv:opt:%s non riconosciuto", argv[4]);
        }
        
        usage("Comando drv:%s non riconosciuto", argv[3]);
    }
    
    usage("Comando %s non riconosciuto", argv[1]);
    return 0;
}

