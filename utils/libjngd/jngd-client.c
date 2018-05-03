/*
 * libjngd.c
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


/// Include

// Robe
#include <stdlib.h>

// Socket UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

// Operazioni file
#include <unistd.h>

// Costanti E*
#include <errno.h>

// Costanti e altro
#include "libjngd.h"



/// Globali

// Indirizzo di jngd
static struct sockaddr_un _daemon_addr = {
    .sun_family = AF_UNIX,
    .sun_path   = SOCKET_FILE
};
static unsigned int _daemon_addr_len = sizeof(_daemon_addr);


// Socket verso jngd
static int _daemon_fd = -1;



/// Funzioni statiche

// Se non si è già connessi a jngd prova a connettersi. Se si è connessi ritorna 0
static int _check_jngd_connection(){
    if(_daemon_fd < 0){
        // Prova a connettersi
        int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if(fd < 0) return -1;
        
        if(connect(fd, (struct sockaddr*)&_daemon_addr, _daemon_addr_len) < 0){
            close(fd);
            return -1;
        }
        _daemon_fd = fd;
    }
    return 0;
}


// Tenta di inviare un pacchetto a jngd
static int _jngd_send(const unsigned char* buffer, size_t len){
    // Controlla la connessione
    if(_check_jngd_connection()) return -1;
    
    // Invia il pacchetto
    int ret = send(_daemon_fd, buffer, len, 0);
    
    if(ret < 0){
        close(_daemon_fd);
        _daemon_fd = -1;
    }
    return ret;
}


// Riceve un pacchetto da jngd
static inline int _jngd_recv(unsigned char* buffer, size_t len){
    if(_check_jngd_connection()) return -1;
    
    int ret = recv(_daemon_fd, buffer, len, 0);
    
    if(ret < 0){
        close(_daemon_fd);
        _daemon_fd = -1;
    }
    return ret;
}



/// Funzioni esportate

// Avvia un driver
int jngd_driver_launch(const char* driver, const char* argv[]){
    unsigned char buffer[32768];
    
    buffer[0] = JNGD_ACTION_DRV_LAUNCH;
    
    buffer[1] = (unsigned char)strlen(driver) + 1;
    strcpy((char*)buffer + 3, driver);
    
    buffer[2] = 0;
    char** arg;
    char*  next_arg = (char*)buffer + 3 + buffer[1];
    for(arg = (char**)argv;*arg;arg++){
        strcpy(next_arg, *arg);
        
        int len = strlen(*arg) + 1;
        buffer[2] += len;
        next_arg  += len;
    }
    
    int ret = _jngd_send(buffer, 3 + buffer[1] + buffer[2]);
    if(ret < 0) return -EIO;
    
    ret = _jngd_recv(buffer, 1);
    if(ret < 0)  return -EIO;
    if(ret == 1) return -EPROTO;
    
    return buffer[0];
}


// Lista dei driver
int jngd_driver_list(char*** list){
    unsigned char buffer[32768];
    int i, n, ret;
    
    buffer[0] = JNGD_ACTION_DRV_LIST;
    
    // Invia la richiesta
    ret = _jngd_send(buffer, 1);
    if(ret < 0) return -EIO;
    
    // Ottiene la risposta
    ret = _jngd_recv(buffer, 32768);
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    // Controlla lo stato
    if(buffer[0] != 0) return -buffer[0];
    if(ret < 3) return -EPROTO;
    
    // Lunghezza del pacchetto
    short len = *(short*)(buffer + 1);
    if(len - 3 > ret) return -EPROTO;
    
    // Conta gli argomenti
    int num = 0;
    for(i = 3;i < len + 3;i++){
        if(buffer[i] == 0) num++;
    }
    
    // Crea la lista
    char** argv = malloc(sizeof(char*) * (num + 1));
    if(argv == NULL) return -ENOMEM;
    
    char*  last_buffer_ptr = (char*)buffer + 3;
    size_t argv_len        = sizeof(char*) * (num + 1);
    
    for(n = 3, i = 0;n < len + 3 && i < num;n++){
        if(buffer[n] == 0){
            // Aggiunge la stringa
            int slen = strlen(last_buffer_ptr) + 1;
            
            argv = realloc(argv, argv_len + slen);
            if(argv == NULL) return -ENOMEM;
            
            // Copia la stringa
            memcpy((char*)argv + argv_len, last_buffer_ptr, slen);
            
            // Mette l'offset della stringa da argv nell'indice
            // Dato che realloc() può cambiare la posizione di argv
            // l'indice viene scritto alla fine della copia
            argv[i++] = (char*)argv_len;
            
            // Aggiorna i contatori
            argv_len += slen;
            last_buffer_ptr += slen;
        }
    }
    num = i;
    
    // Fine della lista
    argv[i] = NULL;
    
    // Aggiorna lgi indici
    for(i = 0;i < num;i++){
        argv[i] = (char*)argv + (size_t)argv[i];
    }
    
    *list = argv;
    
    return 0;
}


// Aggiorna i driver installati
int jngd_drvoption_update(){
    unsigned char buffer[1];
    
    buffer[0] = JNGD_ACTION_DRVOPT_UPDATE;
    if(_jngd_send(buffer, 1) < 0) return -EIO;
    
    int ret = _jngd_recv(buffer, 1);
    if(ret < 0) return -EIO;
    if(ret != 1) return -EPROTO;
    
    return -buffer[0];
}


// Lista opzioni
int jngd_drvoption_list(jngd_option_t** list){
    unsigned char buffer[65536];
    int i, n;
    
    buffer[0] = JNGD_ACTION_DRVOPT_LIST;
}
