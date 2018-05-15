/*
 * joystick-ng-control-fops.c
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/string.h>

#include "joystick-ng-core.h"

// Protocollo control

static int jng_control_open(struct inode* in, struct file* fp){
    int a = 0;
    int b = 1;
    
    // Per segnalare le code dopo aver rilasciato gli spinlock
    int wake_a_driver = 0;
    int wake_b_driver = 0;
    
    // Swap 0 e 1
    
    // Blocca l'accesso ai joystick
    spin_lock(&jng_joysticks_lock);
    
    // Niente da scambiare
    if(jng_joysticks[a].driver == NULL && jng_joysticks[b].driver == NULL){
        spin_unlock(&jng_joysticks_lock);
        return -EACCES;
    }
    
    write_lock(&jng_joysticks[a].state_lock);
    spin_lock(&jng_joysticks[a].feedback_lock);
    
    write_lock(&jng_joysticks[b].state_lock);
    spin_lock(&jng_joysticks[b].feedback_lock);
    
    // Uno slot è vuoto
    if(jng_joysticks[a].driver == NULL || jng_joysticks[b].driver == NULL){
        // Ottieni il nuovo indice del joystick connesso e disconnesso 
        int n_conn    = (jng_joysticks[a].driver)? b : a;
        int n_notconn = (n_conn == a)? b : a;
        
        
        // Aggiorna il joystick 'ora connesso'
        jng_joysticks[n_conn].driver    = jng_joysticks[n_notconn].driver;
        memcpy(&jng_joysticks[n_conn].info, &jng_joysticks[n_notconn].info, sizeof(jng_info_t));
        
        jng_joysticks[n_conn].driver->joystick = jng_joysticks + n_conn;
        jng_joysticks[n_conn].driver->r_inc    = jng_joysticks[n_conn].feedback_inc;
        
        // Genera evento connesso
        jng_joysticks[n_conn].state.connected  = 1;
        jng_joysticks[n_conn].info.connected   = 1;
        
        // Rigenera eventi client e driver
        jng_joysticks[n_conn].state_inc++;
        jng_joysticks[n_conn].feedback_inc++;
        
        
        // Aggiorna il joystick 'ora disconnesso'
        jng_joysticks[n_notconn].driver = NULL;
        
        // Genera evento connesso
        jng_joysticks[n_notconn].state.connected  = 0;
        jng_joysticks[n_notconn].info.connected   = 0;
        
        // Rigenera eventi client e driver
        jng_joysticks[n_notconn].state_inc++;
        jng_joysticks[n_notconn].feedback_inc++;
        
        // Risveglia solo l'unico driver che può essere in attesa
        wake_a_driver = n_conn == a;
        wake_b_driver = n_conn == b;
    } else {
        // a e b non NULL
        jng_connection_t* tmp;
        jng_info_t tmpinfo;
        
        // Scambia i driver
        tmp = jng_joysticks[a].driver;
        jng_joysticks[a].driver = jng_joysticks[b].driver;
        jng_joysticks[b].driver = tmp;
        
        // Cambia le info
        memcpy(&tmpinfo,               &jng_joysticks[a].info, sizeof(jng_info_t));
        memcpy(&jng_joysticks[a].info, &jng_joysticks[b].info, sizeof(jng_info_t));
        memcpy(&jng_joysticks[b].info, &tmpinfo,               sizeof(jng_info_t));
        
        // Aggiorna i joystick a cui puntano i driver
        jng_joysticks[a].driver->joystick = jng_joysticks + b;
        jng_joysticks[a].driver->r_inc    = jng_joysticks[b].feedback_inc;
        
        jng_joysticks[b].driver->joystick = jng_joysticks + a;
        jng_joysticks[b].driver->r_inc    = jng_joysticks[a].feedback_inc;
        
        // Rigenera eventi client e driver per a e b
        jng_joysticks[a].state_inc++;
        jng_joysticks[a].feedback_inc++;
        jng_joysticks[b].state_inc++;
        jng_joysticks[b].feedback_inc++;
        
        wake_a_driver = 1;
        wake_b_driver = 1;
    }
    
    // Rilascia gli spinlock in ordine inverso
    spin_unlock(&jng_joysticks[b].feedback_lock);
    write_unlock(&jng_joysticks[b].state_lock);
    
    spin_unlock(&jng_joysticks[a].feedback_lock);
    write_unlock(&jng_joysticks[a].state_lock);
    
    spin_unlock(&jng_joysticks_lock);
    
    // Risveglia i driver solo se necessario
    if(wake_a_driver){
        wake_up_interruptible(&jng_joysticks[a].feedback_queue);
    }
    
    if(wake_b_driver){
        wake_up_interruptible(&jng_joysticks[b].feedback_queue);
    }
    
    // Risveglia i client per permettere l'aggiornamento dei dati
    wake_up_interruptible(&jng_joysticks[a].state_queue);
    wake_up_interruptible(&jng_joysticks[b].state_queue);
    
    return -EPERM;
}


static ssize_t jng_control_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    return -EINVAL;
}


static ssize_t jng_control_write(struct file* fp, const char __user* buffer, size_t len, loff_t* offp){
    return -EINVAL;
}


static int jng_control_release(struct inode* in, struct file* fp){
    return -EINVAL;
}

// Le operazioni
struct file_operations joystick_ng_control_fops = {
    // Con owner impostato il reference counter del modulo viene aggiornato automaticamente, qui abbiamo technology
    .owner          = THIS_MODULE,
    
    .open           = jng_control_open,
    
    .read           = jng_control_read,
    .write          = jng_control_write,
    
    .release        = jng_control_release,
    
    .llseek         = no_llseek
};
