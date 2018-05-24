/*
 * list.c
 * 
 * Copyright 2016-2018 Fabio Meneghetti <fabiomene97@gmail.com>
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

// Gestione di liste dinamiche, versione kernel/joystick-ng

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/spinlock.h>

#include "list.h"

#define LOCK() spin_lock(&l->access_spinlock)
#define RET_UNLOCK(v) do{spin_unlock(&l->access_spinlock); return (v);}while(0)

void jng_list_init(jng_list_t* l, unsigned int es, unsigned int ql){
    spin_lock_init(&l->access_spinlock);
    
    l->buffer = NULL;
    l->buflen = 0;
    
    l->element_len   = es;
    l->element_limit = ql;
}


int jng_list_append(jng_list_t* l, void* el){
    unsigned char* new_ptr;
    
    LOCK();
    
    // Controlla la dimensione massima
    if(l->element_limit && l->buflen == l->element_limit) RET_UNLOCK(-1);
    
    // Alloca spazio per il nuovo elemento
    new_ptr = krealloc(l->buffer, (++l->buflen) * l->element_len, GFP_KERNEL);
    
    if(!new_ptr){
        l->buflen--;
        RET_UNLOCK(-1);
    }
    
    // Aggiorna il puntatore
    l->buffer = new_ptr;
    
    // Copia il nuovo elemento
    memcpy(l->buffer + (l->buflen - 1) * l->element_len, el, l->element_len);
    
    RET_UNLOCK(0);
}


int jng_list_pop(jng_list_t* l, void* dest){
    LOCK();
    
    // Sanity check
    if(l->buflen == 0) RET_UNLOCK(-1);
    
    // Copia l'elemento
    memcpy(dest, l->buffer, l->element_len);
    
    // Sposta tutti gli elementi indietro di un posto
    if(l->buflen != 1){
        memmove(l->buffer, l->buffer + l->element_len, (l->buflen - 1) * l->element_len);
    }
    
    // Rialloca lo spazio
    if(--l->buflen == 0){
        kfree(l->buffer);
        l->buffer = NULL;
    } else {
        // Sperando che krealloc non possa fallire se si diminuisce la dimensione
        l->buffer = krealloc(l->buffer, l->buflen * l->element_len, GFP_KERNEL);
    }
    
    RET_UNLOCK(0);
}


int jng_list_get(jng_list_t* l, void* dest, unsigned int pos){
    LOCK();
    
    // Sanity check
    if(pos >= l->buflen) RET_UNLOCK(-1);
    
    // Copia
    memcpy(dest, l->buffer + pos * l->element_len, l->element_len);
    
    RET_UNLOCK(0);
}

unsigned int jng_list_len(jng_list_t* l){
    unsigned int len;
    
    spin_lock(&l->access_spinlock);
        len = l->buflen;
    spin_unlock(&l->access_spinlock);
    
    return len;
}

void jng_list_iter(jng_list_t* l, void(*cb)(void*, void*), void* arg){
    unsigned int i;
    
    spin_lock(&l->access_spinlock);
    
    for(i = 0;i < l->buflen;i++){
        cb(l->buffer + i * l->element_len, arg);
    }
    
    spin_unlock(&l->access_spinlock);
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

int jng_list_del(jng_list_t* l, unsigned int pos){
    LOCK();
    
    // Sanity check
    if(pos < 0) RET_UNLOCK(-1);
    if(pos >= l->buflen) RET_UNLOCK(-1);
    
    // Se l'elemento è l'ultimo della lista non bisogna spostare tutto, basta riallocare
    if(pos != l->buflen - 1)
        memmove(
            l->buffer + pos * l->element_len,        // dst
            l->buffer + (pos + 1) * l->element_len,  // src
            (l->buflen - pos - 1) * l->element_len   // len
        );
    
    // Libera o rialloca lo spazio (sempre sperando che krealloc non possa fallire)
    if(--l->buflen == 0){
        kfree(l->buffer);
        
        l->buffer = NULL;
    } else {
        l->buffer = krealloc(l->buffer, l->buflen * l->element_len, GFP_KERNEL);
    }
    
    RET_UNLOCK(0);
}


int jng_list_delcb(jng_list_t* l, int(*cb)(void*, void*), void* arg){
    unsigned int i, n = 0;
    int exit_cycle = 0; // Unsigned e overflow, belle cose
    
    // Non è possibile chiamare jng_list_del dato che andrebbe in deadlock
    LOCK();
    
    // Controlla che la lista contenga almeno un elemento
    if(l->buflen == 0) RET_UNLOCK(0);
    
    for(i = l->buflen - 1;exit_cycle == 0;i--){
        if(cb(l->buffer + i * l->element_len, arg)){
            // Copiato spudoratamente da jng_list_del
            if(i != l->buflen - 1)
                memmove(
                    l->buffer + i * l->element_len,
                    l->buffer + (i + 1) * l->element_len,
                    (l->buflen - i - 1) * l->element_len
                );
                
            // "Segna" l'elemento come eliminato
            l->buflen--;
            
            n++;
        }
        
        if(i == 0) exit_cycle = 1;
    }
    
    // Sistema lo spazio allocato
    if(l->buflen == 0){
        kfree(l->buffer);
        
        l->buffer = NULL;
    } else {
        l->buffer = krealloc(l->buffer, l->buflen * l->element_len, GFP_KERNEL);
    }
    
    RET_UNLOCK(n);
}


void jng_list_delall(jng_list_t* l){
    spin_lock(&l->access_spinlock);
    
    kfree(l->buffer);
    l->buffer = NULL;
    l->buflen = 0;
    
    spin_unlock(&l->access_spinlock);
}

