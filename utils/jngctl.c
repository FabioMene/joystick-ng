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

#include "libjngd-client/libjngd.h"


void usage(char* fmt, ...){
    printf("jngctl, interfaccia di controllo per jngd\n"
           "Comandi disponibili:\n"
           "  jngctl dlist\n"
           "    Stampa una lista dei driver installati\n"
           "\n"
           "  jngctl dlaunch <driver> [argomenti...]\n"
           "    Avvia un driver (In genere usato con udev)\n"
           "\n"
           "\n"
           "formato opzioni\n"
           "  opzione         Opzione globale\n"
           "  driver.opzione  Opzione specifica del driver/Override globale\n"
           "\n"
           "  jngctl opt list [driver]\n"
           "    Stampa le opzioni del driver e la loro descrizione\n"
           "    Se driver non è specificato stampa le opzioni globali\n"
           "\n"
           "  jngctl opt get <opzione|driver.|driver.opzione>\n"
           "    Stampa il valore dell'opzione. Se si specifica solo driver. (punto letterale)\n"
           "    stampa i valori di tutte le opzioni del driver\n"
           "\n"
           "  jngctl opt set <opzione|driver.opzione> <val>\n"
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

#define CHECK_RET(ret) do{if((ret) < 0){printf("Errore nell'esecuzione del comando: %s\n", strerror(-(ret)));return 1;}}while(0)

int main(int argc, char* argv[]){
    if(argc < 2) usage(NULL);
    
    int i, ret;
    
    if(strcmp(argv[1], "dlist") == 0){
        char** list;
        
        ret = jngd_driver_list(&list);
        CHECK_RET(ret);
        
        char** cursor;
        ret = 0;
        
        for(cursor = list;*cursor;cursor++, ret++){
            printf("%s\n", *cursor);
        }
        
        if(!ret) printf("Nessun driver installato\n");
        
        free(list);
        
        return 0;
    }
    
    if(strcmp(argv[1], "dlaunch") == 0){
        if(argc < 3) usage("Il comando dlaunch richiede almeno un argomento");
        
        ret = jngd_driver_launch(argv[2], argv + 3);
        
        CHECK_RET(ret);
        
        return 0;
    }
    
    if(strcmp(argv[1], "opt") == 0){
        if(argc < 3) usage("Il comando opt richiede almeno un argomenti");
        
        if(strcmp(argv[2], "list") == 0){
            jngd_option_t* opts;
            
            // Uso argv[3] perché stringa o NULL va bene tutto
            ret = jngd_drvoption_list(argv[3], &opts);
            
            CHECK_RET(ret);
            
            return 0;
        }
    }
    
    usage("Comando %s non riconosciuto", argv[1]);
    return 1;
    /*
    
        
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
    return 0;*/
}

