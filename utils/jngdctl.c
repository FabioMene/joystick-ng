/*
 * jngdctl.c
 * 
 * Copyright 2017 Fabio Meneghetti <fabiomene97@gmail.com>
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

// jngdctl
// Questo programma permette di lanciare i driver di joystick-ng e
// di modificarne le relative impostazioni

// Da jngdriverd
#define SOCKET_FILE "/var/run/jngdriverd.socket"

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
#include "libjngdsett/libjngdsett.h"


void usage(char* fmt, ...){
    printf("jngdctl, interfaccia di controllo per i driver di joystick-ng\n"
           "Comandi disponibili:\n"
           "  jngdctl start <driver> [argomenti...]    # Avvia un driver di joystick-ng, tramite jngdriverd\n"
           "  jngdctl list                             # Stampa la lista dei driver installati\n"
           "  jngdctl <driver> opt list                # Stampa la lista di opzioni del driver\n"
           "  jngdctl <driver> opt get <opzione>       # Stampa l'opzione\n"
           "  jngdctl <driver> opt set <opzione> <val> # Imposta l'opzione\n"
           "  jngdctl help                             # Questo aiuto\n");
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

int main(int argc, char* argv[]){
    if(argc < 2) usage(NULL);
    
    if(strcmp(argv[1], "start") == 0){
        if(argc < 3) usage("start richiede almeno un argomento");
        
        int ret = jngdsett_load(argv[2]);
        if(ret < 0){
            printf("Impossibile caricare la definizione del driver\n"
                   "Il driver potrebbe non essere installato\n");
            return 1;
        }
        
        unsigned char buffer[32768];
        
        ret = jngdsett_read("exec", buffer + 3);
        if(ret < 0){
            printf("Impossibile leggere il percorso dell'eseguibile, file di definizione incompleto\n");
            return 1;
        }
        
        buffer[0] = (unsigned char)strlen((char*)(buffer + 3));
        buffer[1] = (unsigned char)strlen(argv[2]);
        buffer[2] = (unsigned char)(argc - 3);
        
        strncpy((char*)buffer + 3 + buffer[0] + 1, argv[2], 255);
        
        int i;
        char* argstart = (char*)buffer + 3 + buffer[0] + 1 + buffer[1] + 1;
        
        for(i = 3;i < argc && i < 256;i++){
            strncpy(argstart, argv[i], 32767 - ((long)argstart - (long)buffer));
            argstart += strlen(argv[i]) + 1;
        }
        
        int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if(sockfd < 0){
            printf("Impossibile creare il socket locale\n");
            return 1;
        }
        
        struct sockaddr_un addr;
        
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, SOCKET_FILE);
        
        if(sendto(sockfd, buffer, (long)argstart - (long)buffer, 0, (struct sockaddr*)&addr, sizeof(addr.sun_family) + strlen(addr.sun_path) + 1) < 0){
            printf("Impossibile inviare al socket locale.\nAssicurarsi che jngdriverd sia attivo e di avere i permessi adeguati\n");
            return 1;
        }
        return 0;
    } else if(strcmp(argv[1], "list") == 0){
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
    } else if(strcmp(argv[1], "help") == 0) usage(NULL);
    // Da qui solo comandi opt
    if(argc < 4) usage("Pochi argomenti");
    if(strcmp(argv[2], "opt")) usage("Comando '%s' non riconosciuto", argv[2]);
    
    if(jngdsett_load(argv[1]) < 0){
        printf("Impossibile caricare le impostazioni del driver\n");
        return 1;
    }
    
    if(strcmp(argv[3], "list") == 0){
        int num, i;
        jngdsett_opt_t* options = jngdsett_optdata(&num);
        
        printf("Opzioni del driver:\n");
        
        char* types[] = {
            "int",
            "double",
            "string"
        };
        
        for(i = 0;i < num;i++){
            if(options[i].type != JNGDSETT_TYPE_EXEC) printf("%s (%s): %s\n", options[i].name, types[options[i].type], options[i].description);
        }
        return 0;
    }
    
    if(strcmp(argv[3], "get") == 0){
        if(argc < 5) usage("opt get richiede un argomento");
        
        int num, i;
        jngdsett_opt_t* options = jngdsett_optdata(&num);
        
        for(i = 0;i < num;i++){
            if(strcmp(options[i].name, argv[4]) == 0) switch(options[i].type){
                case JNGDSETT_TYPE_INT: {
                        int val;
                        sscanf(options[i].value, "%i", &val);
                        printf("%d\n", val);
                    } return 0;
                case JNGDSETT_TYPE_DOUBLE: {
                        double val;
                        sscanf(options[i].value, "%lf", &val);
                        printf("%lf\n", val);
                    } return 0;
                case JNGDSETT_TYPE_STRING:
                case JNGDSETT_TYPE_EXEC:
                    printf("%s\n", options[i].value);
                    return 0;
            }
        }
        
        printf("L'opzione non esiste\n");
        return 1;
    }
    
    if(strcmp(argv[3], "set") == 0){
        if(argc < 6) usage("opt set richiede due argomenti");
        
        int res = jngdsett_write(argv[4], argv[5]);
        if(res < 0){
            printf("Impossibile scrivere le impostazioni, assicurarsi di avere i permessi adeguati\n");
            return 1;
        }
        if(res){
            printf("L'opzione non esiste\n");
            return 1;
        }
        return 0;
    }
    
    usage(NULL);
    return 0;
}

