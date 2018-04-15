/*
 * jngdsett.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "libjngdsett.h"


static jngdsett_opt_t* optdata = NULL;
static int             optnum  = 0;

static char            last_drv_name[1024] = "";

#define rsscanf(n, line, fmt...) do{if(sscanf((char*)line, fmt) != n){return 1;}}while(0)
    
static int _analyze_def_line(unsigned char* line){
    if(strlen((char*)line) == 0 || line[0] == '#') return 0;
    
    jngdsett_opt_t opt;
    
    rsscanf(1, line, "%255[^ =]", opt.name);
    
    if(strcmp(opt.name, "exec") == 0){
        // Il campo exec è particolare
        opt.type = JNGDSETT_TYPE_EXEC;
        rsscanf(1, line, "%*255[^ =]%*[ =]%255s", opt.value);
    } else {
        // Ottieni il tipo
        char type[256];
        rsscanf(1, line, "%*255[^ =]%*[ =]%255s", type);
        if     (strcmp(type, "int")    == 0) opt.type = JNGDSETT_TYPE_INT;
        else if(strcmp(type, "double") == 0) opt.type = JNGDSETT_TYPE_DOUBLE;
        else if(strcmp(type, "string") == 0) opt.type = JNGDSETT_TYPE_STRING;
        else return 1;
        
        rsscanf(2, line, "%*255[^ =]%*[ =]%*255[^ |]%*[ ]|%[^|]%*[ |]%511[^|]", opt.value, opt.description);
    }
    
    // Cerca duplicati
    int i;
    for(i = 0;i < optnum;i++){
        if(strcmp(optdata[i].name, opt.name) == 0){
            memcpy(optdata + i, &opt, sizeof(jngdsett_opt_t));
            return 0;
        }
    }
    
    // Crea l'opzione
    optdata = realloc(optdata, sizeof(jngdsett_opt_t) * (optnum + 1));
    if(!optdata) return -1;
    
    memcpy(optdata + optnum, &opt, sizeof(jngdsett_opt_t));
    
    optnum++;
    
    return 0;
}

static int _analyze_setting_line(unsigned char* line){
    if(strlen((char*)line) == 0) return 0;
    
    char name[256];
    char value[256];
    
    rsscanf(2, line, "%255[^=]=%255[^|]", name, value);
    
    
    int i;
    for(i = 0;i < optnum;i++){
        if(strcmp(optdata[i].name, name) == 0){
            break;
        }
    }
    if(i == optnum){
        // L'opzione non esiste
        return 1;
    }
    
    union {
        int i;
        double d;
    } sink;
    
    // Test valore
    switch(optdata[i].type){
        case JNGDSETT_TYPE_INT:
            rsscanf(1, value, "%i", &sink.i);
            break;
        case JNGDSETT_TYPE_DOUBLE:
            rsscanf(1, value, "%lf", &sink.d);
            break;
    }
    
    memcpy(optdata[i].value, value, 256); 
    
    return 0;
}

static int _analyze_file(char* path, int(*cb)(unsigned char*)){
    unsigned char* buffer = NULL;
    int            len = 0;
    int            res, i, n;
    int            fd = open(path, O_RDONLY);
    if(fd < 0) return -1;
    
    do {
        len += 256;
        buffer = realloc(buffer, len);
        if(buffer == NULL){
            close(fd);
            return -1;
        }
        res = read(fd, buffer + len - 256, 256);
        if(res < 0){
            close(fd);
            return -1;
        }
    } while(res == 256);
    close(fd);
    
    len -= 255 - res;
    
    buffer = realloc(buffer, len);
    if(buffer == NULL) return -1;
    
    buffer[len - 1] = 0;
    
    // Separa e analizza le linee
    unsigned char* line = buffer;
    
    int dirty = 0;
    
    for(i = 0;i < len;i++){
        if(buffer[i] == '\n'){
            buffer[i] = 0;
            
            // Togli whitespace all'inizio
            n = 0;
            sscanf((char*)line, "%*[ ]%n", &n);
            if(n) memmove(line, line + n, strlen((char*)(line + n)));
            
            dirty |= cb(line);
            
            line = buffer + i + 1;
        }
    }
    cb(line);
    
    free(buffer);
    
    return 0;
}

int jngdsett_load(char* name){
    // Ottieni il nome
    if(name == NULL){
        name = getenv("JNG_DRIVER");
        if(name == NULL) return -1;
    }
    strncpy(last_drv_name, name, 1023);
    
    // Carica le definizioni
    int dirty;
    
    char path[1024];
    strcpy(path, "/etc/jngd/defs/");
    strncat(path, name, 1023);
    dirty  = _analyze_file(path, _analyze_def_line);
    if(dirty < 0) return -1;
    
    strcpy(path, "/etc/jngd/settings/");
    strncat(path, name, 1023);
    dirty |= _analyze_file(path, _analyze_setting_line) & 0x01;
    
    return dirty;
}


int jngdsett_read(char* opt, void* dest){
    // Cerca l'opzione
    int i;
    for(i = 0;i < optnum;i++){
        if(strcmp(optdata[i].name, opt) == 0){
            break;
        }
    }
    if(i == optnum){
        // L'opzione non esiste
        return 1;
    }
    
    switch(optdata[i].type){
        case JNGDSETT_TYPE_INT:
            sscanf(optdata[i].value, "%i", (int*)dest);
            break;
        case JNGDSETT_TYPE_DOUBLE:
            sscanf(optdata[i].value, "%lf", (double*)dest);
            break;
        case JNGDSETT_TYPE_STRING:
        case JNGDSETT_TYPE_EXEC:
            strcpy(dest, optdata[i].value);
            break;
    }
    
    return 0;
}


int jngdsett_write(char* opt, const char* val){
    // Aggiorna le opzioni da disco
    jngdsett_load(last_drv_name);
    
    // Trova e aggiorna l'opzione
    int i;
    for(i = 0;i < optnum;i++){
        if(strcmp(optdata[i].name, opt) == 0){
            break;
        }
    }
    if(i == optnum){
        // L'opzione non esiste
        return 1;
    }
    
    union {
        int i;
        double d;
    } sink;
    
    // Test valore
    switch(optdata[i].type){
        case JNGDSETT_TYPE_INT:
            rsscanf(1, val, "%i", &sink.i);
            break;
        case JNGDSETT_TYPE_DOUBLE:
            rsscanf(1, val, "%lf", &sink.d);
            break;
    }
    
    strncpy(optdata[i].value, val, 255);
    
    // Scrivi il file
    char path[1024];
    strcpy(path, "/etc/jngd/settings/");
    strncat(path, last_drv_name, 1023);
    
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if(fd < 0) return -1;
    
    for(i = 0;i < optnum;i++){
        if(optdata[i].type != JNGDSETT_TYPE_EXEC){
            write(fd, optdata[i].name, strlen(optdata[i].name));
            write(fd, "=", 1);
            write(fd, optdata[i].value, strlen(optdata[i].value));
            write(fd, "\n", 1);
        }
    }
    
    close(fd);
    return 0;
}

jngdsett_opt_t* jngdsett_optdata(int* num){
    *num = optnum;
    return optdata;
}

void jngdsett_reset(){
    free(optdata);
    optdata = NULL;
    optnum = NULL;
    last_drv_name[0] = 0;
}
