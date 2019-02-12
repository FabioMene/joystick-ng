/*
 * joystick-ng-control-fops.c
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

// Protocollo control (molto stateless)

static int jng_control_open(struct inode* in, struct file* fp){
    return 0;
}

// Niente read e write
static ssize_t jng_control_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    return -EINVAL;
}

static ssize_t jng_control_write(struct file* fp, const char __user* buffer, size_t len, loff_t* offp){
    return -EINVAL;
}

static int jng_control_release(struct inode* in, struct file* fp){
    return 0;
}


// Prototipi
static int jng_ctrl_swdisconnect(uint32_t slot);
static int jng_ctrl_swap(uint32_t a, uint32_t b);


static long jng_control_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
    printi("ioctl ctrl %d(%ld)", cmd, arg);
    
    switch(cmd){
        case JNGCTRLIOCSWDISC:
            return jng_ctrl_swdisconnect(arg);
        
        case JNGCTRLIOCSWAPJS: {
            uint32_t slots[2];
            if(copy_from_user(slots, (void*)arg, sizeof(uint32_t[2]))) return -EFAULT;
            
            return jng_ctrl_swap(slots[0], slots[1]);
        }
    }
    
    return -ENOTTY;
}


// Funzioni

static int jng_ctrl_swdisconnect(uint32_t slot){
    int wake = 0;
    
    if(slot > JNG_TOT - 1) return -EINVAL;
    
    spin_lock(&jng_joysticks_lock);
        jng_control_wlock();
        
            if(jng_joysticks[slot].driver){
                jng_joysticks[slot].driver->joystick = NULL;
                jng_joysticks[slot].driver = NULL;
                
                jng_joysticks[slot].state_ex.control.connected = 0;
                jng_joysticks[slot].info.connected             = 0;
                
                jng_state_wlock(jng_joysticks + slot);
                    jng_joysticks[slot].state_inc++;
                jng_state_wunlock(jng_joysticks + slot);
                
                jng_feedback_lock(jng_joysticks + slot);
                    jng_joysticks[slot].feedback_inc++;
                jng_feedback_unlock(jng_joysticks + slot);
                
                wake = 1;
            }
        
        jng_control_wunlock();
    spin_unlock(&jng_joysticks_lock);
    
    if(wake){
        wake_up_interruptible(&jng_joysticks[slot].state_queue);
        wake_up_interruptible(&jng_joysticks[slot].feedback_queue);
    }
    
    return 0;
}

static int jng_ctrl_swap(uint32_t a, uint32_t b){
    // Per segnalare le code dopo aver rilasciato gli spinlock
    int wake_driver_a = 0;
    int wake_driver_b = 0;
    
    // Sanity check
    if(a > JNG_TOT - 1 || b > JNG_TOT - 1) return -EINVAL;
    if(a == b) return -EINVAL;
    
    jng_control_wlock();
    
    // Blocca l'accesso ai joystick
    spin_lock(&jng_joysticks_lock);
    
    // Niente da scambiare
    if(jng_joysticks[a].driver == NULL && jng_joysticks[b].driver == NULL){
        spin_unlock(&jng_joysticks_lock);
        jng_control_wunlock();
        return 0;
    }
    
    write_lock(&jng_joysticks[a].state_lock);
    spin_lock(&jng_joysticks[a].feedback_lock);
    
    write_lock(&jng_joysticks[b].state_lock);
    spin_lock(&jng_joysticks[b].feedback_lock);
    
    // Uno slot è vuoto
    if(jng_joysticks[a].driver == NULL || jng_joysticks[b].driver == NULL){
        // Ottieni il nuovo indice del joystick connesso e disconnesso 
        uint32_t n_conn    = (jng_joysticks[a].driver)? b : a;
        uint32_t n_notconn = (n_conn == a)? b : a;
        
        
        // Aggiorna il joystick 'ora connesso'
        jng_joysticks[n_conn].driver = jng_joysticks[n_notconn].driver;
        memcpy(&jng_joysticks[n_conn].info, &jng_joysticks[n_notconn].info, sizeof(jng_info_t));
        
        jng_joysticks[n_conn].driver->joystick = jng_joysticks + n_conn;
        jng_joysticks[n_conn].driver->r_inc    = jng_joysticks[n_conn].feedback_inc;
        
        // JNG_CTRL_CONNECTION
        jng_joysticks[n_conn].state_ex.control.connected = 1;
        
        // JNG_CTRL_INFO_CHANGED
        jng_joysticks[n_conn].state_ex.control.last_info_inc = jng_joysticks[n_conn].state_inc + 1;
        
        // Notifica che ci sono nuovi eventi di stato
        jng_joysticks[n_conn].state_inc++;
        
        // JNG_CTRL_SLOT_CHANGED
        jng_joysticks[n_conn].feedback_ex.control.slot = n_conn;
        jng_joysticks[n_conn].feedback_inc++;
        
        
        // Aggiorna il joystick 'ora disconnesso'
        jng_joysticks[n_notconn].driver = NULL;
        
        // JNG_CTRL_CONNECTION
        jng_joysticks[n_notconn].info.connected = 0;
        jng_joysticks[n_notconn].state_ex.control.connected = 0;
        jng_joysticks[n_notconn].state_inc++;
        
        
        // Risveglia solo l'unico driver che può essere in attesa
        wake_driver_a = n_conn == a;
        wake_driver_b = n_conn == b;
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
        jng_joysticks[a].driver->joystick = jng_joysticks + a;
        jng_joysticks[a].driver->r_inc    = jng_joysticks[a].feedback_inc;
        
        jng_joysticks[b].driver->joystick = jng_joysticks + b;
        jng_joysticks[b].driver->r_inc    = jng_joysticks[b].feedback_inc;
        
        // JNG_CTRL_INFO_CHANGED
        jng_joysticks[a].state_ex.control.last_info_inc = jng_joysticks[a].state_inc + 1;
        jng_joysticks[b].state_ex.control.last_info_inc = jng_joysticks[b].state_inc + 1;
        
        // JNG_CTRL_SLOT_CHANGED
        jng_joysticks[a].feedback_ex.control.slot = a;
        jng_joysticks[b].feedback_ex.control.slot = b;
        
        // Rigenera eventi client e driver per a e b
        jng_joysticks[a].state_inc++;
        jng_joysticks[a].feedback_inc++;
        
        jng_joysticks[b].state_inc++;
        jng_joysticks[b].feedback_inc++;
        
        wake_driver_a = 1;
        wake_driver_b = 1;
    }
    
    // Rilascia gli spinlock in ordine inverso
    spin_unlock(&jng_joysticks[b].feedback_lock);
    write_unlock(&jng_joysticks[b].state_lock);
    
    spin_unlock(&jng_joysticks[a].feedback_lock);
    write_unlock(&jng_joysticks[a].state_lock);
    
    spin_unlock(&jng_joysticks_lock);
    
    jng_control_wunlock();
    
    // Risveglia i driver solo se necessario
    if(wake_driver_a){
        wake_up_interruptible(&jng_joysticks[a].feedback_queue);
    }
    
    if(wake_driver_b){
        wake_up_interruptible(&jng_joysticks[b].feedback_queue);
    }
    
    // Risveglia i client per permettere l'aggiornamento dei dati
    wake_up_interruptible(&jng_joysticks[a].state_queue);
    wake_up_interruptible(&jng_joysticks[b].state_queue);
    
    return 0;
}


// Le operazioni
struct file_operations joystick_ng_control_fops = {
    // Con owner impostato il reference counter del modulo viene aggiornato automaticamente, qui abbiamo technology
    .owner          = THIS_MODULE,
    
    .open           = jng_control_open,
    
    .read           = jng_control_read,
    .write          = jng_control_write,
    
    .unlocked_ioctl = jng_control_ioctl,
    .compat_ioctl   = jng_control_ioctl,
    
    .release        = jng_control_release,
    
    .llseek         = no_llseek
};
