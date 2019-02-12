/*
 * drvoption-ops.c
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

// Gestisce le operazioni DRVOPT_*

#include <stdlib.h>

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "jngd.h"

int do_drvopt_update(unsigned char* packet __attribute__((unused)), int* len){
    *len = 0;
    return drvoption_load();
}


int do_drvopt_list(unsigned char* packet, int* len){
    CHECK_SIZE(1);
    
    internal_option_t* opts;
    internal_option_t* opt;
    int                optn;
    
    int ret;
    
    if(packet[0] != 0){
        packet[packet[0] + 1] = 0;
        ret = drvoption_dump_list((char*)packet + 1, &opts, &optn);
    } else {
        ret = drvoption_dump_list(NULL, &opts, &optn);
    }
    
    if(ret < 0){
        return (ret == -1)?ENOENT:ENOMEM;
    }
    
    int idx = 2, i;
    int n = 0;
    
    for(i = 0;i < optn;i++){
        opt = opts + i;
        
        // Skippa eseguibili e opzioni globali sovrascritte
        if(opt->type == JNGD_DRVOPT_TYPE_EXEC) continue;
        if(opt->is_override) continue;
        
        // Check dimensioni
        if(idx + 5 > 65535) break;
        
        // Header
        packet[idx + 0]                      = strlen(opt->name) + 1;
        packet[idx + 1]                      = opt->type;
        packet[idx + 2]                      = strlen(opt->def_value) + 1;
        *(unsigned short*)(packet + idx + 3) = strlen(opt->description) + 1;
        
        // Check dimensioni (di nuovo)
        if(idx + 5 + packet[idx+0] + packet[idx+2] + *(unsigned short*)(packet+idx+3) > 65535) break;
        
        // Copia stringhe
        memcpy(packet +idx+5,                               opt->name,        packet[idx + 0]);
        memcpy(packet +idx+5 +packet[idx+0],                opt->def_value,   packet[idx + 2]);
        memcpy(packet +idx+5 +packet[idx+0] +packet[idx+2], opt->description, *(unsigned short*)(packet+idx+3));
        
        // Aggiorna indice
        idx += 5 + packet[idx + 0] + packet[idx + 2] + *(unsigned short*)(packet + idx + 3);
        
        n++;
    }
    
    free(opts);
    
    *(unsigned short*)packet = n;
    *len = idx;
    
    return 0;
}


int do_drvopt_get(unsigned char* packet, int* len){
    char opt[256];
    
    CHECK_SIZE(1);
    CHECK_SIZE(1 + packet[0]);
    
    memcpy(opt, packet + 1, packet[0]);
    
    if(drvoption_resolve_option(opt, (char*)packet + 1) < 0) return ENOENT;
    
    packet[0] = strlen((char*)packet + 1) + 1;
    *len = 1 + packet[0];
    
    return 0;
}


int do_drvopt_set(unsigned char* packet, int* len){
    CHECK_SIZE(2);
    CHECK_SIZE(2 + packet[0] + packet[1]);
    
    char opt[256];
    char val[256];
    
    memcpy(opt, packet + 2,             packet[0]);
    memcpy(val, packet + 2 + packet[0], packet[1]);
    
    int ret = drvoption_update_option(opt, val);
    
    // Comunque non ritorniamo dati
    *len = 0;
    
    if(ret == -1) return ENOENT; // Opzione non trovata
    if(ret <  -1) return EIO; // Errore salvataggio
    
    return 0;
}

