/*
 * js-ops.c
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

// Gestisce le azioni JS_*

#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>

#include "jngd.h"
#include "../../include/joystick-ng.h"

int do_js_soft_disconnect(unsigned char* packet, int* len){
    // Controlli pacchetti
    CHECK_SIZE(4);
    
    // Ritorna sempre e solo lo stato
    *len = 0;
    
    int fd = open(CONTROL_DEVICE, O_RDWR);

    if(fd < 0){
        printe("[JS_SOFT_DISCONNECT] Impossibile aprire il control device: %d", errno);
        return errno;
    }
    
    int ret = -ioctl(fd, JNGCTRLIOCSWDISC, *(uint32_t*)packet);
    
    close(fd);
    
    return ret;
}


int do_js_swap(unsigned char* packet, int* len){
    // Controlli pacchetti
    CHECK_SIZE(8);
    
    *len = 0;
    
    int fd = open(CONTROL_DEVICE, O_RDWR);

    if(fd < 0){
        printe("[JS_SWAP] Impossibile aprire il control device: %d", errno);
        return errno;
    }
    
    int ret = -ioctl(fd, JNGCTRLIOCSWAPJS, packet);
    
    close(fd);
    
    return ret;
}

