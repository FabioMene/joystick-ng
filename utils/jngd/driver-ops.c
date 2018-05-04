/*
 * driver-ops.c
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

// Gestisce le azioni DRV_*

#include <stdlib.h>

#include <unistd.h>
#include <string.h>
#include <fcntl.h>


int do_drv_launch(unsigned char* packet, int* len){
    // Controlli pacchetti
    CHECK_SIZE(2);
    CHECK_SIZE(2 + packet[0] + packet[1]);
    
    // Sanity check
    packet[2 + packet[0] - 1] = 0;
    packet[2 + packet[0] + packet[1] - 1] = 0;
    
    int i, n;
    
    char exec_path[512];
    i = drvoption_read_exec((char*)packet + 2);
    
    if(i){
        printe("[DRV_LAUNCH] Il driver %s non ha exec", packet + 2);
        return -ENOENT;
    }
    
    // Creazione vettore argomenti
    int argc = 2; // Minimo (eseguibile, NULL)
    for(i = 2 + packet[0];i < len;i++){
        if(packet[i] == 0) argc++;
    }
    
    char** new_argv = malloc(argc * sizeof(char*));
    if(!new_argv){
        printe("[DRV_LAUNCH] Memoria esaurita");
        return -ENOMEM;
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
}

