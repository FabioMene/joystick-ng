/*
 * joystick-ng-driver-fops.c
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

// device driver per la connessione con il driver

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/string.h>
//#include <asm-generic/uaccess.h> boh, a quanto pare viene importato quello specifico dell'architettura da qualche parte

#include "joystick-ng-core.h"

static int jng_driver_open(struct inode* in, struct file* fp){
    // ISO C90 di staminchia
    int _error;
    int i;
    
    printi("Nuova connessione con driver");
    // Macro per uscire e robba
    #define _open_fail(op, err, lbl) do{printe("Errore apertura connessione driver: " # op);_error=err;goto open_ ## lbl ## _fail;}while(0)
    // Alloca memoria (fp->private_data = jng_connection_data)
    fp->private_data = kmalloc(sizeof(jng_connection_t), GFP_KERNEL);
    if(!jng_connection_data) _open_fail(kmalloc, ENOMEM, );
    
    // Trova un joystick libero
    spin_lock(&jng_joysticks_lock);
    for(i = 0;i < JNG_TOT;i++){
        if(jng_joysticks[i].driver == NULL){
            jng_joysticks[i].driver          = jng_connection_data;
            jng_joysticks[i].state.connected = 1;
            jng_joysticks[i].info.connected  = 1;
            jng_joysticks[i].state_inc++; // Questo viene considerato evento di controllo
            break;
        }
    }
    spin_unlock(&jng_joysticks_lock);
    if(i == JNG_TOT) _open_fail(no joystick free, EMFILE, free_joystick);
    
    // Trovato il joystick, ultime cose
    jng_connection_data->joystick = jng_joysticks + i;
    jng_connection_data->mode     = JNG_MODE_NORMAL;
    jng_connection_data->evmask   = JNG_EV_FB_FORCE | JNG_EV_FB_LED;
    
    jng_queue_init(&jng_connection_data->rbuffer, sizeof(jng_event_t), JNG_MAX_CONN_EV);
    jng_connection_data->r_inc = 0;
    
    wake_up_interruptible(&jng_connection_data->joystick->state_queue); // Per l'evento di controllo
    
    printi("Nuovo driver: slot assegnato %d", jng_connection_data->joystick->num);
    return 0;
    
  open_free_joystick_fail:
    kfree(fp->private_data);
  open__fail:
    return -_error;
    
    #undef _open_fail
}

static int jng_update_driver_incremental(jng_connection_t* conn){
    int return_val = -1;
    
    jng_feedback_lock(conn);
    if(conn->r_inc != conn->joystick->feedback_inc){
        memcpy(&conn->tmpfeedback, &conn->joystick->feedback, sizeof(jng_feedback_t));
        return_val = 0;
        conn->r_inc = conn->joystick->feedback_inc;
    }
    jng_feedback_unlock(conn);
    
    return return_val;
}

static void jng_gen_driver_events(jng_connection_t* conn){
    // Questa macro evita 5000000 milioni di if
    #define jng_diff_cmp(_prop, _type, _what) if(conn->diff.feedback._prop != conn->tmpfeedback._prop) jng_add_event(conn, _type, _what, conn->tmpfeedback._prop)
    
    // Force feedback
    if(conn->evmask & JNG_EV_FB_FORCE){
        jng_diff_cmp(force.bigmotor,    JNG_EV_FB_FORCE, JNG_FB_FORCE_BIGMOTOR);
        jng_diff_cmp(force.smallmotor,  JNG_EV_FB_FORCE, JNG_FB_FORCE_SMALLMOTOR);
        jng_diff_cmp(force.extramotor1, JNG_EV_FB_FORCE, JNG_FB_FORCE_EXTRAMOTOR1);
        jng_diff_cmp(force.extramotor2, JNG_EV_FB_FORCE, JNG_FB_FORCE_EXTRAMOTOR2);
    }
    
    if(conn->evmask & JNG_EV_FB_LED){
        jng_diff_cmp(leds.led1, JNG_EV_FB_LED, JNG_FB_LED_1);
        jng_diff_cmp(leds.led2, JNG_EV_FB_LED, JNG_FB_LED_2);
        jng_diff_cmp(leds.led3, JNG_EV_FB_LED, JNG_FB_LED_3);
        jng_diff_cmp(leds.led4, JNG_EV_FB_LED, JNG_FB_LED_4);
    }
    
    #undef jng_diff_cmp
    
    // Aggiornamento diff
    memcpy(&conn->diff.feedback, &conn->tmpfeedback, sizeof(jng_feedback_t));
}

static ssize_t jng_driver_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // Modalità ad eventi
        
        if(len < sizeof(jng_event_t)) return -EINVAL;
        
      jng_drv_rd_read_again:
        
        // Controlla se lo stato feedback è cambiato dall'ultimo controllo,
        // e se sì, genera eventuali nuovi eventi
        if(jng_update_driver_incremental(jng_connection_data) == 0){
            jng_gen_driver_events(jng_connection_data);
        }
        
        // È possibile che non ci sia nessun evento, in tal caso controlla cosa fare
        if(jng_queue_pop(&jng_connection_data->rbuffer, &jng_connection_data->tmpevent)){
            if(fp->f_flags & O_NONBLOCK) return -EAGAIN;
            // Dobbiamo aspettare
            if(wait_event_interruptible(jng_connection_data->joystick->feedback_queue, jng_connection_data->r_inc != jng_connection_data->joystick->feedback_inc)) return -ERESTARTSYS;
            // Controlliamo di nuovo i dati
            goto jng_drv_rd_read_again;
        }
        // Finalmente abbiamo un evento
        // copiamolo in user space e ritorniamo
        return copy_to_user(buffer, &jng_connection_data->tmpevent, sizeof(jng_event_t)) ? -EFAULT : sizeof(jng_event_t);
    }
    
    // Mod normale se arriviamo qui
    if(len < sizeof(jng_feedback_t)) return -EINVAL;
    // copy_to_user può andare in sleep, ergo non si può usare con uno spinlock bloccato
    // La procedura è fblock --> joystick->tmp --> fbunlock --> tmp->user
    jng_feedback_lock(jng_connection_data);
    memcpy(&jng_connection_data->tmpfeedback, &jng_connection_data->joystick->feedback, sizeof(jng_feedback_t));
    jng_feedback_unlock(jng_connection_data);
    return copy_to_user(buffer, &jng_connection_data->tmpfeedback, sizeof(jng_feedback_t)) ? -EFAULT : sizeof(jng_feedback_t);
}

static ssize_t jng_driver_write(struct file* fp, const char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->mode & JNG_WMODE_EVENT){
        if(len < sizeof(jng_event_t)) return -EINVAL;
        if(copy_from_user(&jng_connection_data->tmpevent, buffer, sizeof(jng_event_t)) != 0) return -EFAULT;
        
        // Solo un driver è collegato ad un dato joystick in un dato momento, ergo non crea problemi l'uso di
        // rlock --> joystick->cache --> runlock --> aggiornamento --> wlock --> cache->joystick --> wunlock --> return 
        jng_state_rlock(jng_connection_data);
        memcpy(&jng_connection_data->tmpstate, &jng_connection_data->joystick->state, sizeof(jng_state_t));
        jng_state_runlock(jng_connection_data);
        
        switch(jng_connection_data->tmpevent.type){
            case JNG_EV_KEY:
                // Sanitizzazione
                jng_connection_data->tmpevent.what &= __JNG_KEY_MASK;
                // Inutile farsi problemi per la pressione se il joystick non la supporta
                if(jng_connection_data->joystick->info.flags & JNG_FLAG_KEY_PRESSURE) JNG_KEYP(jng_connection_data->tmpstate, jng_connection_data->tmpevent.what) = jng_connection_data->tmpevent.value;
                else                                                                  JNG_KEYP(jng_connection_data->tmpstate, jng_connection_data->tmpevent.what) = (jng_connection_data->tmpevent.value)?255:0;
                
                if(jng_connection_data->tmpevent.value) jng_connection_data->tmpstate.keys |=  jng_connection_data->tmpevent.what;
                else                                    jng_connection_data->tmpstate.keys &= ~jng_connection_data->tmpevent.what;
                break;
            case JNG_EV_AXIS:
                jng_connection_data->tmpevent.what &= __JNG_AXIS_MASK;
                JNG_AXIS(jng_connection_data->tmpstate, jng_connection_data->tmpevent.what) = jng_connection_data->tmpevent.value;
                break;
            case JNG_EV_SENSOR:
                jng_connection_data->tmpevent.what &= __JNG_SEN_MASK;
                JNG_SENSOR(jng_connection_data->tmpstate, jng_connection_data->tmpevent.what) = jng_connection_data->tmpevent.value;
                break;
        }
        jng_state_wlock(jng_connection_data);
        memcpy(&jng_connection_data->joystick->state, &jng_connection_data->tmpstate, sizeof(jng_state_t));
        jng_connection_data->joystick->state_inc++;
        jng_state_wunlock(jng_connection_data);
        
        wake_up_interruptible(&jng_connection_data->joystick->state_queue);
        
        return sizeof(jng_event_t);
    }
    // Mod normale
    if(len < sizeof(jng_state_t)) return -EINVAL;
    // stesso problema di read, copy_from_user può attendere
    // quindi la procedura è user->tmp --> wlock --> tmp->joystick --> wunlock
    if(copy_from_user(&jng_connection_data->tmpstate, buffer, sizeof(jng_state_t)) != 0) return -EFAULT;
    jng_connection_data->tmpstate.connected = 1;
    jng_state_wlock(jng_connection_data);
    memcpy(&jng_connection_data->joystick->state, &jng_connection_data->tmpstate, sizeof(jng_state_t));
    jng_connection_data->joystick->state_inc++;
    jng_state_wunlock(jng_connection_data);
    wake_up_interruptible(&jng_connection_data->joystick->state_queue);
    return sizeof(jng_state_t);
}

static unsigned int jng_driver_poll(struct file* fp, poll_table* pt){
    // Se il driver blocca o no in lettura dipende dalla modalità
    // in scrittura invece non blocca mai
    unsigned int mask = POLLOUT | POLLWRNORM;
    
    // Aggiungi comunque la coda eventi
    poll_wait(fp, &jng_connection_data->joystick->feedback_queue, pt);
    
    // Controlla la mod di lettura
    // In mod eventi se c'è almeno un evento in coda ritorna "leggibile"
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // In modalità eventi fa gli stessi controlli di read()
        // e ritorna POLLIN solo se c'è almeno un evento
        
        if(jng_update_driver_incremental(jng_connection_data) == 0){
            jng_gen_driver_events(jng_connection_data);
        }
        
        if(jng_queue_len(&jng_connection_data->rbuffer) != 0){
            mask |= POLLIN | POLLRDNORM;
        }
    } else {
        // In mod normale la lettura non blocca mai
        mask |= POLLIN | POLLRDNORM;
    }
    
    return mask;
}

static int jng_driver_fsync(struct file* fp, loff_t start, loff_t end, int datasync){
    // Non viene controllata la modalità in lettura
    // Se si è in lettura normale questo non avrà effetto
    
    // Cancella la coda degli eventi
    jng_queue_delall(&jng_connection_data->rbuffer);
    
    // Aggiorna le differenze. Non è necessario controllare ->joystick, dato che viene assegnato dal kernel in open()
    jng_feedback_lock(jng_connection_data);
    memcpy(&jng_connection_data->tmpfeedback, &jng_connection_data->joystick->feedback, sizeof(jng_feedback_t));
    jng_feedback_unlock(jng_connection_data);
    memcpy(&jng_connection_data->diff.feedback, &jng_connection_data->tmpfeedback, sizeof(jng_feedback_t));
    return 0;
}

static int jng_del_unwanted_events_cb(void* el, void* arg){
    return !(*((unsigned int*)arg) & ((jng_event_t*)el)->type);
}

static long jng_driver_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
    int rc;
    
    printi("ioctl driver %d: %d(%ld)", jng_connection_data->joystick->num, cmd, arg);
    switch(cmd){
        case JNGIOCSETSLOT: // Solo client
            return -EINVAL;
        
        case JNGIOCGETSLOT:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->joystick->num, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETINFO:
            jng_state_wlock(jng_connection_data);
            rc = copy_from_user(&jng_connection_data->joystick->info, (jng_info_t*)arg, sizeof(jng_info_t));
            jng_connection_data->joystick->info.connected = 1;
            jng_state_wunlock(jng_connection_data);
            return rc ? -EFAULT : 0;
        
        case JNGIOCGETINFO: // Perchè un driver dovrebbe volere le sue info? mah
            jng_state_rlock(jng_connection_data);
            rc = copy_to_user((jng_info_t*)arg, &jng_connection_data->joystick->info, sizeof(jng_info_t));
            jng_state_runlock(jng_connection_data);
            return rc ?-EFAULT : 0;
        
        case JNGIOCSETMODE:
            jng_connection_data->mode = arg;
            if(!(arg & JNG_RMODE_EVENT)){
                // Quando si esce dalla lettura ad eventi cancella la coda
                jng_queue_delall(&jng_connection_data->rbuffer);
            } else {
                // Se si imposta la lettura ad eventi si azzera la struttura delle differenze, così gli eventi vengono ricreati
                memset(&jng_connection_data->diff.feedback, 0, sizeof(jng_feedback_t));
            }
            return 0;
        
        case JNGIOCGETMODE:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->mode, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETEVMASK:
            jng_connection_data->evmask = arg;
            // Cancella eventuali eventi a cui il driver non è più interessato
            jng_queue_delcb(&jng_connection_data->rbuffer, jng_del_unwanted_events_cb, &arg);
            return 0;
        
        case JNGIOCGETEVMASK:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->evmask, sizeof(unsigned int)) ? -EFAULT : 0;
    }
    return -ENOTTY;
}

static int jng_driver_release(struct inode* in, struct file* fp){
    printi("Connessione con driver per il joystick %d chiusa", jng_connection_data->joystick->num);
    // Rilascio il joystick, poi i dati
    spin_lock(&jng_joysticks_lock);
    jng_connection_data->joystick->driver          = NULL;
    jng_connection_data->joystick->state.connected = 0;
    jng_connection_data->joystick->info.connected  = 0;
    jng_connection_data->joystick->state_inc++; // Questo viene considerato evento di controllo
    memset(&jng_connection_data->joystick->state, 0, sizeof(jng_state_t));
    spin_unlock(&jng_joysticks_lock);
    
    wake_up_interruptible(&jng_connection_data->joystick->state_queue); // Risveglia i client in ascolto
    
    kfree(fp->private_data);
    return 0;
}

// Le operazioni
struct file_operations joystick_ng_driver_fops = {
    // Con owner impostato il reference counter del modulo viene aggiornato automaticamente, qui abbiamo technology
    .owner          = THIS_MODULE,
    
    .open           = jng_driver_open,
    
    .read           = jng_driver_read,
    .poll           = jng_driver_poll,
    .fsync          = jng_driver_fsync,
    .write          = jng_driver_write,
    
    .unlocked_ioctl = jng_driver_ioctl,
    .compat_ioctl   = jng_driver_ioctl,
    
    .release        = jng_driver_release,
    
    .llseek         = no_llseek
};

