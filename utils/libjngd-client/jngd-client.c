/*
 * libjngd-client.c
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

// sscanf
#include <stdio.h>

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
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    return -buffer[0];
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
    if(ret < 1) return -EPROTO;
    
    return -buffer[0];
}


// Lista opzioni
int jngd_drvoption_list(const char* driver, jngd_option_t** list){
    unsigned char buffer[65536];
    int i, n;

    // Invia richiesta
    buffer[0] = JNGD_ACTION_DRVOPT_LIST;
    buffer[1] = 0;
    
    if(driver != NULL){
        buffer[1] = (strlen(driver) + 1) & 0xff;
        memcpy((char*)buffer + 2, driver, buffer[1]);
    }
    
    if(_jngd_send(buffer, 2 + buffer[1]) < 0) return -EIO;
    
    // Riceve la risposta
    int ret = _jngd_recv(buffer, 65536);
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    if(buffer[0] != 0) return -buffer[0];
    
    unsigned short num = *(unsigned short*)(buffer + 1);
    
    // Alloca l'array delle opzioni
    size_t options_len = sizeof(jngd_option_t) * (num + 1);
    
    jngd_option_t* options = malloc(options_len);
    if(options == NULL) return -ENOMEM;
    
    // Parsing risposta. Come per jngd_driver_list le stringhe vengono messe alla fine di questo array
    // e di conseguenza realloc() può interferire con questi puntatori, che per ora vengono riempiti con offset
    for(i = 0, n = 3;i < num;){
        // Almeno lungo quanto l'header
        if(ret < n + 5) break;
        
        unsigned char      nlen = buffer[n + 0];
        unsigned char      vlen = buffer[n + 2];
        unsigned short     dlen = *(unsigned short*)(buffer + n + 3);
        
        // Almeno lungo tutta l'opzione
        if(ret < n + 5 + nlen + vlen + dlen) break;
        
        // Fai spazio per le stringhe
        options = realloc(options, options_len + nlen + vlen + dlen);
        if(options == NULL) return -ENOMEM;
        
        // Copia le stringhe
        memcpy((char*)options + options_len,               buffer + n + 5,               nlen);
        memcpy((char*)options + options_len + nlen,        buffer + n + 5 + nlen,        vlen);
        memcpy((char*)options + options_len + nlen + vlen, buffer + n + 5 + nlen + vlen, dlen);
        
        // Aggiorna gli offset (e il tipo dell'opzione)
        options[i].name        = (char*)options_len;
        options[i].type        = buffer[n + 1];
        options[i].def         = (char*)(options_len + nlen);
        options[i].description = (char*)(options_len + nlen + vlen);
        
        // Aggiorna la lunghezza di options
        options_len += nlen + vlen + dlen;
        
        // Aggiorna gli indici
        n += 5 + nlen + vlen + dlen;
        i += 1;
    }
    
    num = i + 1;
    options[i].type = JNGD_DRVOPT_TYPE_END;
    
    for(i = 0;i < num;i ++){
        options[i].name        = (char*)options + (size_t)options[i].name;
        options[i].def         = (char*)options + (size_t)options[i].def;
        options[i].description = (char*)options + (size_t)options[i].description;
    }
    
    *list = options;
    
    return 0;
}


// Il nome driver per drvoption_get
static char driver_name[256] = "";

// Imposta il nome driver per drvoption_get
// Questo permette di mantenere una certa compatibilità con libjngdsett
// e la variabile d'ambiente JNG_DRIVER. Questo prefisso viene usato solo da drvoption_get
void jngd_set_drvoption_driver(const char* driver){
    if(driver){
        strncpy(driver_name, driver, 255);
        return;
    }
    
    char* drv = getenv("JNG_DRIVER");
    if(drv){
        strncpy(driver_name, drv, 255);
    }
}

// Ottieni un opzione
int jngd_drvoption_get(const char* option, jngd_option_type_e type, void* dst){
    char drvoption[256];
    if(strlen(driver_name)){
        strcpy(drvoption, driver_name);
        strcat(drvoption, ".");
        strcat(drvoption, option);
    } else {
        strcpy(drvoption, option);
    }
    
    unsigned char buffer[258];
    
    // Invia richiesta
    buffer[0] = JNGD_ACTION_DRVOPT_GET;
    buffer[1] = (strlen(drvoption) + 1) & 0xff;
    memcpy(buffer + 2, drvoption, buffer[1]);
    
    if(_jngd_send(buffer, 2 + buffer[1]) < 0) return -EIO;
    
    // Ottieni la risposta + controlli
    int ret = _jngd_recv(buffer, 258);
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    if(buffer[0] != 0) return -buffer[0];
    
    if(buffer[1] < ret - 2) return -EPROTO;
    
    // Copia la risposta su dst, con le opportune conversioni
    switch(type){
        case JNGD_DRVOPT_TYPE_INT:
            if(sscanf((char*)buffer + 2, "%d", (int*)dst) != 1) return -EINVAL;
            return 0;
        
        case JNGD_DRVOPT_TYPE_DOUBLE:
            if(sscanf((char*)buffer + 2, "%lf", (double*)dst) != 1) return -EINVAL;
            return 0;
        
        default:
            // Se il tipo è stringa o exec copia soltanto
            break;
    }
    
    memcpy(dst, buffer + 2, buffer[1]);
    
    return 0;
}


// Imposta un opzione
int jngd_drvoption_set(const char* option, jngd_option_type_e type, const void* src){
    unsigned char buffer[515];
    
    // Crea la richiesta
    buffer[0] = JNGD_ACTION_DRVOPT_SET;
    buffer[1] = (strlen(option) + 1) & 0xff;
    memcpy(buffer + 3, option, buffer[1]);
    
    switch(type){
        case JNGD_DRVOPT_TYPE_INT:
            buffer[2] = snprintf((char*)buffer + 3 + buffer[1], 256, "%d", *(int*)src);
            break;
        
        case JNGD_DRVOPT_TYPE_DOUBLE:
            buffer[2] = snprintf((char*)buffer + 3 + buffer[1], 256, "%lf", *(double*)src);
            break;
        
        default:
            // Tratta src come stringa 0-terminata
            buffer[2] = strlen((char*)src);
            strcpy((char*)buffer + 3 + buffer[1], (char*)src);
            break;
    }
    
    // Aggiungi il terminatore
    buffer[3 + buffer[1] + buffer[2]] = 0;
    buffer[2]++;
    
    // Invia richiesta
    if(_jngd_send(buffer, 3 + buffer[1] + buffer[2]) < 0) return -EIO;
    
    // Ottieni la risposta
    int ret = _jngd_recv(buffer, 1);
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    return -buffer[0];
}


// Disconnetti un joystick
int jngd_js_soft_disconnect(uint32_t slot){
    unsigned char buffer[5];
    
    // Crea la richiesta
    buffer[0] = JNGD_ACTION_JS_SOFT_DISCONNECT;
    memcpy(buffer + 1, &slot, 4);
    
    // Invia richiesta
    if(_jngd_send(buffer, 5) < 0) return -EIO;
    
    // Ottieni la risposta
    int ret = _jngd_recv(buffer, 1);
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    return -buffer[0];
}

// Scambia due joystick
extern int jngd_js_swap(uint32_t slot_a, uint32_t slot_b){
    unsigned char buffer[9];
    
    // Crea la richiesta
    buffer[0] = JNGD_ACTION_JS_SWAP;
    memcpy(buffer + 1, &slot_a, 4);
    memcpy(buffer + 5, &slot_b, 4);
    
    // Invia richiesta
    if(_jngd_send(buffer, 9) < 0) return -EIO;
    
    // Ottieni la risposta
    int ret = _jngd_recv(buffer, 1);
    if(ret < 0) return -EIO;
    if(ret < 1) return -EPROTO;
    
    return -buffer[0];
}

