/*
 * queue.c
 * 
 * Copyright 2016-2017 Fabio Meneghetti <fabiomene97@gmail.com>
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

// Gestione di code dinamiche, versione kernel/joystick-ng

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gfp.h>

#include "queue.h"

void jng_queue_init(jng_queue_t* q, int es, int ql){
    q->buffer = NULL;
    q->buflen = 0;
    q->element_len   = es;
    q->element_limit = ql;
}


int jng_queue_add(jng_queue_t* q, void* el){
    if(q->buflen == q->element_limit) return -1;
    q->buffer = krealloc(q->buffer, (++q->buflen) * q->element_len, GFP_KERNEL);
    if(!q->buffer){
        q->buffer = NULL;
        q->buflen = 0;
        return -1;
    }
    memcpy(q->buffer + (q->buflen - 1) * q->element_len, el, q->element_len);
    return 0;
}


int jng_queue_pop(jng_queue_t* q, void* dest){
    if(q->buflen == 0) return -1;
    memcpy(dest, q->buffer, q->element_len);
    if(q->buflen != 1){
        memmove(q->buffer, q->buffer + q->element_len, (q->buflen - 1) * q->element_len);
    }
    if(--q->buflen == 0){
        kfree(q->buffer);
        q->buffer = NULL;
    } else {
        q->buffer = krealloc(q->buffer, q->buflen * q->element_len, GFP_KERNEL);
    }
    return 0;
}


int jng_queue_get(jng_queue_t* q, void* dest, int pos){
    if(pos < 0) return -1;
    if(pos >= q->buflen) return -1;
    memcpy(dest, q->buffer + pos * q->element_len, q->element_len);
    return 0;
}

unsigned int jng_queue_len(jng_queue_t* q){
    return q->buflen;
} 

//   _ _ _ _ _ 
//  |a|b|c|d|e|
//  0 1 2 3 4
//  del 2 (3 -> 2, len = buflen - pos - 1)
//   _ _ _ _
//  |a|b|d|e|
//  0 1 2 3
//
//  

int jng_queue_del(jng_queue_t* q, int pos){
    if(pos < 0) return -1;
    if(pos >= q->buflen) return -1;
    if(pos != q->buflen - 1) memmove(q->buffer + pos * q->element_len, q->buffer + (pos + 1) * q->element_len, (q->buflen - pos - 1) * q->element_len);
    q->buffer = krealloc(q->buffer, (--q->buflen) * q->element_len, GFP_KERNEL);
    if(q->buflen == 0) q->buffer = NULL;
    return 0;
}


int jng_queue_delcb(jng_queue_t* q, int(*cb)(void*, void*), void* arg){
    int i, n = 0;
    for(i = q->buflen - 1;i >= 0;i--){
        if(cb(q->buffer + i * q->element_len, arg)){
            jng_queue_del(q, i);
            n++;
        }
    }
    return n;
}


void jng_queue_delall(jng_queue_t* q){
    kfree(q->buffer);
    q->buffer = NULL;
    q->buflen = 0;
}

