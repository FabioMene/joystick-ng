/*
 * joystick-ng-client-fops.c
 * 
 * Copyright 2015-2017 Fabio Meneghetti <fabiomene97@gmail.com>
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

// Device driver per client (il file è una brutta copia di joystick-ng-driver-dops.c)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/gfp.h>
//#include <asm-generic/uaccess.h>

#include "joystick-ng-core.h"

static int jng_client_open(struct inode* in, struct file* fp){
    int _error;
    
    printi("Nuova connessione con client");
    #define _open_fail(op, err, lbl) do{printe("Errore apertura connessione client: " # op);_error=err;goto open_ ## lbl ## _fail;}while(0)
    // Alloca memoria (fp->private_data = jng_connection_data)
    fp->private_data = kmalloc(sizeof(jng_connection_t), GFP_KERNEL);
    if(!jng_connection_data) _open_fail(kmalloc, ENOMEM, );
    
    // Il joystick va selezionato prima, tramite ioctl
    jng_connection_data->joystick = NULL;
    jng_connection_data->mode     = JNG_MODE_NORMAL;
    jng_connection_data->evmask   = JNG_EV_KEY | JNG_EV_AXIS | JNG_EV_CTRL; // Di default non riceve gli eventi dei sensori
    
    jng_queue_init(&jng_connection_data->rbuffer, sizeof(jng_event_t), JNG_MAX_CONN_EV);
    jng_connection_data->r_inc = 0;
    
    return 0;

  open__fail:
    return -_error;
    
    #undef _open_fail
}

static void jng_cl_gen_ev(jng_connection_t* conn, unsigned short type, unsigned int what, int val){
    conn->tmpevent.type  = type;
    conn->tmpevent.what  = what;
    conn->tmpevent.value = val;
    jng_queue_add(&conn->rbuffer, &conn->tmpevent);
}

static ssize_t jng_client_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->joystick == NULL) return -EINVAL;
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        int newdata;
        
        if(len < sizeof(jng_event_t)) return -EINVAL;
        
      jng_cl_rd_read_again:
        newdata = 0;
        // Copia nella cache + aggiorna incrementale
        jng_state_rlock();
        if(jng_connection_data->r_inc != jng_connection_data->joystick->state_inc){
            memcpy(&jng_connection_data->tmpstate, &jng_connection_data->joystick->state, sizeof(jng_state_t));
            newdata = 1;
            jng_connection_data->r_inc = jng_connection_data->joystick->state_inc;
        }
        jng_state_runlock();
        
        if(newdata){
            // Trova le differenze
            #define jng_cl_rd_cmp(prop, type, what) if((jng_connection_data->evmask & type) != 0 && jng_connection_data->diff.state.prop != jng_connection_data->tmpstate.prop) jng_cl_gen_ev(jng_connection_data, type, what, jng_connection_data->tmpstate.prop);
            
            jng_cl_rd_cmp(connected,       JNG_EV_CTRL,   JNG_CTRL_CONNECTED);
            
            jng_cl_rd_cmp(keyp.A,          JNG_EV_KEY,    JNG_KEY_A);
            jng_cl_rd_cmp(keyp.B,          JNG_EV_KEY,    JNG_KEY_B);
            jng_cl_rd_cmp(keyp.C,          JNG_EV_KEY,    JNG_KEY_C);
            jng_cl_rd_cmp(keyp.X,          JNG_EV_KEY,    JNG_KEY_X);
            jng_cl_rd_cmp(keyp.Y,          JNG_EV_KEY,    JNG_KEY_Y);
            jng_cl_rd_cmp(keyp.Z,          JNG_EV_KEY,    JNG_KEY_Z);
            jng_cl_rd_cmp(keyp.L1,         JNG_EV_KEY,    JNG_KEY_L1);
            jng_cl_rd_cmp(keyp.R1,         JNG_EV_KEY,    JNG_KEY_R1);
            jng_cl_rd_cmp(keyp.L2,         JNG_EV_KEY,    JNG_KEY_L2);
            jng_cl_rd_cmp(keyp.R2,         JNG_EV_KEY,    JNG_KEY_R2);
            jng_cl_rd_cmp(keyp.L3,         JNG_EV_KEY,    JNG_KEY_L3);
            jng_cl_rd_cmp(keyp.R3,         JNG_EV_KEY,    JNG_KEY_R3);
            jng_cl_rd_cmp(keyp.L1,         JNG_EV_KEY,    JNG_KEY_L1);
            jng_cl_rd_cmp(keyp.R1,         JNG_EV_KEY,    JNG_KEY_R1);
            jng_cl_rd_cmp(keyp.Down,       JNG_EV_KEY,    JNG_KEY_DOWN);
            jng_cl_rd_cmp(keyp.Right,      JNG_EV_KEY,    JNG_KEY_RIGHT);
            jng_cl_rd_cmp(keyp.Left,       JNG_EV_KEY,    JNG_KEY_LEFT);
            jng_cl_rd_cmp(keyp.Up,         JNG_EV_KEY,    JNG_KEY_UP);
            jng_cl_rd_cmp(keyp.Start,      JNG_EV_KEY,    JNG_KEY_START);
            jng_cl_rd_cmp(keyp.Select,     JNG_EV_KEY,    JNG_KEY_SELECT);
            jng_cl_rd_cmp(keyp.Options1,   JNG_EV_KEY,    JNG_KEY_OPTIONS1);
            jng_cl_rd_cmp(keyp.Options2,   JNG_EV_KEY,    JNG_KEY_OPTIONS2);
            jng_cl_rd_cmp(keyp.Other1,     JNG_EV_KEY,    JNG_KEY_OTHER1);
            jng_cl_rd_cmp(keyp.Other2,     JNG_EV_KEY,    JNG_KEY_OTHER2);
            
            jng_cl_rd_cmp(axis.LX,         JNG_EV_AXIS,   JNG_AXIS_LX);
            jng_cl_rd_cmp(axis.LY,         JNG_EV_AXIS,   JNG_AXIS_LY);
            jng_cl_rd_cmp(axis.RX,         JNG_EV_AXIS,   JNG_AXIS_RX);
            jng_cl_rd_cmp(axis.RY,         JNG_EV_AXIS,   JNG_AXIS_RY);
            jng_cl_rd_cmp(axis.G1,         JNG_EV_AXIS,   JNG_AXIS_G1);
            jng_cl_rd_cmp(axis.G2,         JNG_EV_AXIS,   JNG_AXIS_G2);
            jng_cl_rd_cmp(axis.G3,         JNG_EV_AXIS,   JNG_AXIS_G3);
            jng_cl_rd_cmp(axis.G4,         JNG_EV_AXIS,   JNG_AXIS_G4);
            
            jng_cl_rd_cmp(accelerometer.x, JNG_EV_SENSOR, JNG_SEN_ACCEL_X);
            jng_cl_rd_cmp(accelerometer.y, JNG_EV_SENSOR, JNG_SEN_ACCEL_Y);
            jng_cl_rd_cmp(accelerometer.z, JNG_EV_SENSOR, JNG_SEN_ACCEL_Z);
            jng_cl_rd_cmp(gyrometer.x,     JNG_EV_SENSOR, JNG_SEN_GYRO_X);
            jng_cl_rd_cmp(gyrometer.y,     JNG_EV_SENSOR, JNG_SEN_GYRO_Y);
            jng_cl_rd_cmp(gyrometer.z,     JNG_EV_SENSOR, JNG_SEN_GYRO_Z);
            
            // Aggiornamento differenze
            memcpy(&jng_connection_data->diff.state, &jng_connection_data->tmpstate, sizeof(jng_state_t));
        }
        
        // È possibile che non ci sia nessun evento
        if(jng_queue_pop(&jng_connection_data->rbuffer, &jng_connection_data->tmpevent)){
            if(fp->f_flags & O_NONBLOCK) return -EAGAIN;
            // Dobbiamo aspettare
            if(wait_event_interruptible(jng_connection_data->joystick->state_queue, jng_connection_data->r_inc != jng_connection_data->joystick->state_inc)) return -ERESTARTSYS;
            // Controlliamo di nuovo i dati
            goto jng_cl_rd_read_again;
        }
        
        // Copiamo l'evento in user space
        return copy_to_user(buffer, &jng_connection_data->tmpevent, sizeof(jng_event_t)) ? -EFAULT : sizeof(jng_event_t);
    }
    // Mod normale
    if(len < sizeof(jng_state_t)) return -EINVAL;
    // copy_to_user può andare in sleep
    jng_state_rlock();
    memcpy(&jng_connection_data->tmpstate, &jng_connection_data->joystick->state, sizeof(jng_state_t));
    jng_state_runlock();
    return copy_to_user(buffer, &jng_connection_data->tmpstate, sizeof(jng_state_t)) ? -EFAULT : sizeof(jng_state_t);
}

static ssize_t jng_client_write(struct file* fp, const char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->joystick == NULL) return -EINVAL;
    if(jng_connection_data->mode & JNG_WMODE_EVENT){
        if(len < sizeof(jng_event_t)) return -EINVAL;
        if(copy_from_user(&jng_connection_data->tmpevent, buffer, sizeof(jng_event_t)) != 0) return -EFAULT;
        
        // Più joystick possono essere collegati allo stesso joystick, quindi la strategia adottata in jng_driver_write
        // non può essere utilizzata. Qui il blocco è totale
        jng_feedback_lock();
        switch(jng_connection_data->tmpevent.type){
            case JNG_EV_FB_FORCE:
                jng_connection_data->tmpevent.what &= __JNG_FB_FORCE_MASK;
                JNG_FB_FORCE(jng_connection_data->joystick->feedback, jng_connection_data->tmpevent.what) = jng_connection_data->tmpevent.value;
                break;
            case JNG_EV_FB_LED:
                jng_connection_data->tmpevent.what &= __JNG_FB_LED_MASK;
                JNG_FB_LED(jng_connection_data->joystick->feedback, jng_connection_data->tmpevent.what) = jng_connection_data->tmpevent.value;
                break;
        }
        jng_connection_data->joystick->feedback_inc++;
        jng_feedback_unlock();
        // Risveglia il driver se serve
        wake_up_interruptible(&jng_connection_data->joystick->feedback_queue);
        // fine
        return sizeof(jng_event_t);
    }
    // Mod normale
    if(len < sizeof(jng_feedback_t)) return -EINVAL;
    // copy_from_user può attendere
    // quindi la procedura è user->tmp --> lock --> tmp->joystick --> unlock
    if(copy_from_user(&jng_connection_data->tmpfeedback, buffer, sizeof(jng_feedback_t)) != 0) return -EFAULT;
    jng_feedback_lock();
    memcpy(&jng_connection_data->joystick->feedback, &jng_connection_data->tmpfeedback, sizeof(jng_feedback_t));
    jng_connection_data->joystick->feedback_inc++;
    jng_feedback_unlock();
    wake_up_interruptible(&jng_connection_data->joystick->feedback_queue);
    return sizeof(jng_feedback_t);
}

static unsigned int jng_client_poll(struct file* fp, poll_table* pt){
    // Se il driver blocca o no in lettura dipende dalla modalità
    // in scrittura invece non blocca mai
    unsigned int mask = POLLOUT | POLLWRNORM;
    
    // Aggiungi comunque la coda eventi
    poll_wait(fp, &jng_connection_data->joystick->state_queue, pt);
    
    // Controlla la mod di lettura
    // In mod eventi se c'è almeno un evento in coda ritorna "leggibile"
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        if(jng_queue_len(&jng_connection_data->rbuffer)) mask |= POLLIN | POLLRDNORM;
    } else  mask |= POLLIN | POLLRDNORM;
    return mask;
}

static int jng_client_flush(struct file* fp, fl_owner_t id){
    // Non viene controllata la modalità in lettura
    // Se si è in lettura normale questo non avrà effetto
    /* FIXME: Causa Kernel Oops in alcune circostanze
    // Cancella la coda degli eventi
    jng_queue_delall(&jng_connection_data->rbuffer);
    // Aggiorna le differenze
    jng_state_rlock();
    memcpy(&jng_connection_data->tmpstate, &jng_connection_data->joystick->state, sizeof(jng_state_t));
    jng_state_runlock();
    memcpy(&jng_connection_data->diff.state, &jng_connection_data->tmpstate, sizeof(jng_state_t));*/
    return 0;
}

static int jng_del_unwanted_events_cb(void* el, void* arg){
    return !(*((unsigned int*)arg) & ((jng_event_t*)el)->type);
}

static long jng_client_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
    int rc;
    
    printi("ioctl client: %d(%ld)", cmd, arg);
    switch(cmd){
        case JNGIOCSETSLOT:
            // Cambia joystick, resetta le code, imposta la rotta, accendi i circuiti temporali...
            // Anzi no, cambia solo joystick dato che la coda di differenza serve comunque
            if(arg < 0 || arg > JNG_TOT - 1) return -EINVAL; // Sta cercando di fotterci questo
            jng_connection_data->joystick = jng_joysticks + arg;
            return 0;
        
        case JNGIOCGETSLOT:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->joystick->num, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETINFO: // Solo driver
            return -EINVAL;
        
        case JNGIOCGETINFO:
            if(jng_connection_data->joystick == NULL) return -EINVAL;
            jng_state_rlock();
            rc = copy_to_user((jng_info_t*)arg, &jng_connection_data->joystick->info, sizeof(jng_info_t));
            jng_state_runlock();
            return rc ?-EFAULT : 0;
        
        
        case JNGIOCSETMODE:
            if(jng_connection_data->joystick == NULL) return -EINVAL;
            jng_connection_data->mode = arg;
            if(!(arg & JNG_RMODE_EVENT)){
                // Cancella la coda degli eventi quando si va in mod lettura normale
                jng_queue_delall(&jng_connection_data->rbuffer);
            } else {
                // Azzera le differenze quando si entra in mod lettura eventi
                memset(&jng_connection_data->diff.state, 0, sizeof(jng_state_t));
            }
            return 0;
        
        case JNGIOCGETMODE:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->mode, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETEVMASK:
            jng_connection_data->evmask = arg;
            // Cancella eventuali eventi a cui il client non è più interessato
            jng_queue_delcb(&jng_connection_data->rbuffer, jng_del_unwanted_events_cb, &arg);
            return 0;
        
        case JNGIOCGETEVMASK:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->evmask, sizeof(unsigned int)) ? -EFAULT : 0;
    }
    return -ENOTTY;
}

static int jng_client_release(struct inode* in, struct file* fp){
    printi("Connessione con client chiusa");
    kfree(fp->private_data);
    return 0;
}


struct file_operations joystick_ng_client_fops = {
    .owner          = THIS_MODULE,
    .open           = jng_client_open,
    .read           = jng_client_read,
    .write          = jng_client_write,
    .poll           = jng_client_poll,
    .flush          = jng_client_flush,
    .unlocked_ioctl = jng_client_ioctl,
    .release        = jng_client_release,
    
    .llseek         = no_llseek
};
