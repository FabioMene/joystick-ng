/*
 * jngdctl.c
 * 
 * Copyright 2017-2019 Fabio Meneghetti <fabiomene97@gmail.com>
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
           "  jngctl drv list\n"
           "    Stampa una lista dei driver installati\n"
           "\n"
           "  jngctl drv launch <driver> [argomenti...]\n"
           "    Avvia un driver (In genere usato con udev)\n"
           "\n"
           "  jngctl drv reload\n"
           "    Ricarica le definizioni dei driver dal disco\n"
           "\n"
           "formato opzioni (per i comandi 'opt'):\n"
           "  opzione         Opzione globale\n"
           "  driver.opzione  Opzione specifica del driver/Override globale\n"
           "\n"
           "  jngctl opt list [driver]\n"
           "    Stampa le opzioni del driver e la loro descrizione\n"
           "    Se driver non è specificato stampa le opzioni globali\n"
           "\n"
           "  jngctl opt get <opzione|.|driver.|driver.opzione>\n"
           "    Stampa il valore dell'opzione. Se si specifica solo driver. (punto letterale)\n"
           "    stampa i valori di tutte le opzioni del driver, se si specifica solo .\n"
           "    stampa tutte le opzioni globali\n"
           "\n"
           "  jngctl opt set <opzione|driver.opzione> <val>\n"
           "    Imposta l'opzione\n"
           "\n"
           "  jngctl js disc <n>\n"
           "    Esegui una disconnessione software del joystick n (1-32)\n"
           "\n"
           "  jngctl js swap <a> <b>\n"
           "    Scambia i joystick a e b\n"
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
    
    int ret;
    
    if(strcmp(argv[1], "drv") == 0){
        if(argc < 3) usage("Il comando drv richiede almeno un argomento");
        
        if(strcmp(argv[2], "list") == 0){
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
        
        if(strcmp(argv[2], "launch") == 0){
            if(argc < 4) usage("Il comando drv:launch richiede almeno un argomento");
            
            ret = jngd_driver_launch(argv[3], (const char**)argv + 4);
            
            CHECK_RET(ret);
            
            return 0;
        }
        
        if(strcmp(argv[2], "reload") == 0){
            ret = jngd_drvoption_update();
            
            CHECK_RET(ret);
            
            return 0;
        }
        
        usage("Comando drv:%s non riconosciuto\n", argv[2]);
        return 1;
    }
    
    if(strcmp(argv[1], "opt") == 0){
        if(argc < 3) usage("Il comando opt richiede almeno un argomento");
        
        if(strcmp(argv[2], "list") == 0){
            jngd_option_t* opts;
            
            // Uso argv[3] perché stringa o NULL va bene tutto
            ret = jngd_drvoption_list(argv[3], &opts);
            
            CHECK_RET(ret);
            
            if(argv[3] == NULL){
                printf("Opzioni globali:\n");
            } else {
                printf("Opzioni per %s:\n", argv[3]);
            }
            
            jngd_option_t* opt;
            
            for(opt = opts;opt->type != JNGD_DRVOPT_TYPE_END;opt++){
                char* type_string;
                
                switch(opt->type){
                    case JNGD_DRVOPT_TYPE_INT:
                        type_string = "intero";
                        break;
                    
                    case JNGD_DRVOPT_TYPE_DOUBLE:
                        type_string = "decimale";
                        break;
                    
                    case JNGD_DRVOPT_TYPE_STRING:
                        type_string = "stringa";
                        break;
                    
                    default:
                        type_string = "??";
                        break;
                }
                
                printf("%s (%s)\n", opt->name, type_string);

                char* desc_ptr = opt->description;
                while(1){
                    char* lf_ptr = strchr(desc_ptr, '\n');

                    if(lf_ptr){
                        printf("    %.*s\n", (int)(lf_ptr - desc_ptr), desc_ptr);
                    } else {
                        printf("    %s\n", desc_ptr);
                        break;
                    }

                    // salta il line feed
                    desc_ptr = lf_ptr + 1;
                }
                printf("    [%s]\n\n", opt->def);

            }
            
            free(opts);
            
            return 0;
        }
        
        if(strcmp(argv[2], "get") == 0){
            if(argc < 4) usage("Il comando opt:get richiede un argomento");
            
            char name[256];
            char buffer[256];
            
            char* option = argv[3];
            
            // Tutte le opzioni di questo driver
            if(option[strlen(option) - 1] == '.'){
                option[strlen(option) - 1] = 0;
                
                jngd_option_t* opts;
                jngd_option_t* opt;
                
                if(option[0] != 0){ // Se stiamo elencando driver + globali
                    printf("Opzioni del driver:\n");
                    
                    ret = jngd_drvoption_list(option, &opts);
                
                    CHECK_RET(ret);
                
                    for(opt = opts;opt->type != JNGD_DRVOPT_TYPE_END;opt++){
                        strcpy(name, option); // driver
                        strcat(name, ".");
                        strcat(name, opt->name);
                        
                        ret = jngd_drvoption_get(name, JNGD_DRVOPT_TYPE_STRING, buffer);
                        
                        CHECK_RET(ret);
                        
                        printf("    %s: %s\n", opt->name, buffer);
                    }
                
                    free(opts);
                    
                    printf("Globali per questo driver:\n");
                }
                
                // Elenca le globali con gli override del driver
                ret = jngd_drvoption_list(NULL, &opts);
            
                CHECK_RET(ret);
            
                for(opt = opts;opt->type != JNGD_DRVOPT_TYPE_END;opt++){
                    if(option[0] != 0){
                        strcpy(name, option); // driver
                        strcat(name, ".");
                        strcat(name, opt->name);
                    } else {
                        strcpy(name, opt->name);
                    }
                    
                    ret = jngd_drvoption_get(name, JNGD_DRVOPT_TYPE_STRING, buffer);
                    
                    CHECK_RET(ret);
                    
                    printf("    %s: %s\n", opt->name, buffer);
                }
            
                free(opts);
                
                return 0;
            }
            
            // Solo l'opzione indicata
            ret = jngd_drvoption_get(option, JNGD_DRVOPT_TYPE_STRING, buffer);
            
            CHECK_RET(ret);
            
            printf("%s\n", buffer);
            
            return 0;
        }
        
        if(strcmp(argv[2], "set") == 0){
            if(argc < 5) usage("Il comando opt:set richiede due argomenti");
            
            ret = jngd_drvoption_set(argv[3], JNGD_DRVOPT_TYPE_STRING, argv[4]);
            
            CHECK_RET(ret);
            
            return 0;
        }
        
        usage("Comando opt:%s non riconosciuto", argv[2]);
        return 1;
    }
    
    if(strcmp(argv[1], "js") == 0){
        if(argc < 3) usage("Il comando js richiede almeno un argomento");
        
        if(strcmp(argv[2], "disc") == 0){
            if(argc < 4) usage("Il comando js:disc richiede un argomento");
            
            unsigned int slot;
            
            if(sscanf(argv[3], "%ud", &slot) != 1 || slot < 1 || slot > 32) usage("L'argomento '%s' non è un numero compreso tra 1 e 32", argv[3]);
            
            slot--;
            
            ret = jngd_js_soft_disconnect(slot);
            
            CHECK_RET(ret);
            
            return 0;
        }
        
        if(strcmp(argv[2], "swap") == 0){
            if(argc < 5) usage("Il comando js:swap richiede due argomenti");
            
            unsigned int slota, slotb;
            
            if(sscanf(argv[3], "%ud", &slota) != 1 || slota < 1 || slota > 32) usage("L'argomento '%s' non è un numero compreso tra 1 e 32", argv[3]);
            if(sscanf(argv[4], "%ud", &slotb) != 1 || slotb < 1 || slotb > 32) usage("L'argomento '%s' non è un numero compreso tra 1 e 32", argv[4]);
            
            slota--;
            slotb--;
            
            ret = jngd_js_swap(slota, slotb);
            
            CHECK_RET(ret);
            
            return 0;
        }
        
        usage("Comando js:%s non riconosciuto", argv[2]);
    }
    
    if(strcmp(argv[1], "help") == 0) usage(NULL);
    
    usage("Comando %s non riconosciuto", argv[1]);
    return 1;
}

