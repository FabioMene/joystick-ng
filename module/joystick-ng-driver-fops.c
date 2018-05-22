/*
 * joystick-ng-driver-fops.c
 * 
 * Copyright 2015-2018 Fabio Meneghetti <fabiomene97@gmail.com>
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
    jng_joystick_t* js; // Per evitare strani effetti collaterali da control
    
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
                jng_joysticks[i].driver = jng_connection_data;
                // ctrl: slot
                jng_joysticks[i].feedback_ex.control.slot = jng_joysticks[i].num; // js.num equivale a i, per ora almeno.
                break;
            }
        }
    spin_unlock(&jng_joysticks_lock);

    if(i == JNG_TOT) _open_fail(no joystick free, EMFILE, free_joystick);
    
    // Trovato il joystick, ultime cose
    jng_control_rlock();
        js = jng_connection_data->joystick = jng_joysticks + i;
    jng_control_runlock();
    
    // Default
    jng_connection_data->mode     = JNG_MODE_BLOCK;
    jng_connection_data->evmask   = JNG_EV_CTRL | JNG_EV_FB_FORCE | JNG_EV_FB_LED;
    
    // Coda eventi
    jng_list_init(&jng_connection_data->rbuffer, sizeof(jng_event_t), JNG_MAX_CONN_EV);
    jng_connection_data->r_inc = 0;
    
    // JNG_CTRL_CONNECTION
    jng_state_wlock(js);
        js->state_ex.control.connected = 1;
        js->state_inc++;
    jng_state_wunlock(js);
    
    wake_up_interruptible(&js->state_queue); // Per l'evento di controllo
    
    printi("Nuovo driver: slot assegnato %d", js->num);
    
    return 0;
    
  open_free_joystick_fail:
    kfree(fp->private_data);
  open__fail:
    return -_error;
    
    #undef _open_fail
}

static int jng_update_driver_incremental(jng_connection_t* conn, jng_joystick_t* js){
    int return_val = -1;
    
    jng_feedback_lock(js);
        if(conn->r_inc != js->feedback_inc){
            memcpy(&conn->tmp.feedback_ex, &js->feedback_ex, sizeof(jng_feedback_ex_t));
            return_val = 0;
            conn->r_inc = js->feedback_inc;
        }
    jng_feedback_unlock(js);
    
    return return_val;
}

// Aggiunge un evento sulla coda di lettura
static void jng_add_driver_event(jng_connection_t* conn, unsigned short type, unsigned int what, int val){
    conn->tmp.event.type  = type;
    conn->tmp.event.what  = what;
    conn->tmp.event.value = val;
    jng_list_append(&conn->rbuffer, &conn->tmp.event);
}

static void jng_gen_driver_events(jng_connection_t* conn){
    // Questa macro evita 5000000 milioni di if
    #define jng_diff_cmp(_prop, _type, _what) if(conn->diff.feedback_ex._prop != conn->tmp.feedback_ex._prop) jng_add_driver_event(conn, _type, _what, conn->tmp.feedback_ex._prop)
    
    // Controllo
    if(conn->evmask & JNG_EV_CTRL){
        jng_diff_cmp(control.slot, JNG_EV_CTRL, JNG_CTRL_SLOT_CHANGED);
    }
    
    // Force feedback
    if(conn->evmask & JNG_EV_FB_FORCE){
        jng_diff_cmp(feedback.force.bigmotor,    JNG_EV_FB_FORCE, JNG_FB_FORCE_BIGMOTOR);
        jng_diff_cmp(feedback.force.smallmotor,  JNG_EV_FB_FORCE, JNG_FB_FORCE_SMALLMOTOR);
        jng_diff_cmp(feedback.force.extramotor1, JNG_EV_FB_FORCE, JNG_FB_FORCE_EXTRAMOTOR1);
        jng_diff_cmp(feedback.force.extramotor2, JNG_EV_FB_FORCE, JNG_FB_FORCE_EXTRAMOTOR2);
    }
    
    // Led
    if(conn->evmask & JNG_EV_FB_LED){
        jng_diff_cmp(feedback.leds.led1, JNG_EV_FB_LED, JNG_FB_LED_1);
        jng_diff_cmp(feedback.leds.led2, JNG_EV_FB_LED, JNG_FB_LED_2);
        jng_diff_cmp(feedback.leds.led3, JNG_EV_FB_LED, JNG_FB_LED_3);
        jng_diff_cmp(feedback.leds.led4, JNG_EV_FB_LED, JNG_FB_LED_4);
    }
    
    #undef jng_diff_cmp
    
    // Aggiornamento diff
    memcpy(&conn->diff.feedback_ex, &conn->tmp.feedback_ex, sizeof(jng_feedback_ex_t));
}

static ssize_t jng_driver_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    jng_joystick_t* js;
    jng_control_copy_joystick(js);
    
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // Modalità ad eventi
        
        if(len < sizeof(jng_event_t)) return -EINVAL;
        
      jng_drv_rd_read_again:
        
        // Controlla se lo stato feedback è cambiato dall'ultimo controllo,
        // e se sì, genera eventuali nuovi eventi
        if(jng_update_driver_incremental(jng_connection_data, js) == 0){
            jng_gen_driver_events(jng_connection_data);
        }
        
        // È possibile che non ci sia nessun evento, in tal caso controlla cosa fare
        if(jng_list_pop(&jng_connection_data->rbuffer, &jng_connection_data->tmp.event)){
            if(fp->f_flags & O_NONBLOCK) return -EAGAIN;
            // Dobbiamo aspettare
            if(wait_event_interruptible(js->feedback_queue, jng_connection_data->r_inc != js->feedback_inc)) return -ERESTARTSYS;
            // Controlliamo di nuovo i dati
            goto jng_drv_rd_read_again;
        }
        // Finalmente abbiamo un evento
        // copiamolo in user space e ritorniamo
        return copy_to_user(buffer, &jng_connection_data->tmp.event, sizeof(jng_event_t)) ? -EFAULT : sizeof(jng_event_t);
    }
    
    // Mod normale se arriviamo qui
    if(len < sizeof(jng_feedback_t)) return -EINVAL;
    
    // Differenzia tra blocco esteso e normale
    if(len >= sizeof(jng_feedback_ex_t)) len = sizeof(jng_feedback_ex_t);
    else                                 len = sizeof(jng_feedback_t);
    
    // copy_to_user può andare in sleep, ergo non si può usare con uno spinlock bloccato
    // La procedura è fblock --> joystick->tmp --> fbunlock --> tmp->user
    jng_feedback_lock(js);
        memcpy(&jng_connection_data->tmp.feedback_ex, &js->feedback_ex, len);
    jng_feedback_unlock(js);
    
    return copy_to_user(buffer, &jng_connection_data->tmp.feedback_ex, len) ? -EFAULT : len;
}

static ssize_t jng_driver_write(struct file* fp, const char __user* buffer, size_t len, loff_t* offp){
    jng_joystick_t* js;
    jng_control_copy_joystick(js);
    
    if(jng_connection_data->mode & JNG_WMODE_EVENT){
        if(len < sizeof(jng_event_t)) return -EINVAL;
        if(copy_from_user(&jng_connection_data->tmp.event, buffer, sizeof(jng_event_t)) != 0) return -EFAULT;
        
        // Solo un driver è collegato ad un dato joystick in un dato momento, ergo non crea problemi l'uso di
        // rlock --> joystick->cache --> runlock --> aggiornamento --> wlock --> cache->joystick --> wunlock --> return 
        jng_state_rlock(js);
            memcpy(&jng_connection_data->tmp.state_ex.state, &js->state_ex.state, sizeof(jng_state_t));
        jng_state_runlock(js);
        
        switch(jng_connection_data->tmp.event.type){
            case JNG_EV_KEY:
                // Sanitizzazione
                jng_connection_data->tmp.event.what &= __JNG_KEY_MASK;
                // Inutile farsi problemi per la pressione se il joystick non la supporta
                if(js->info.flags & JNG_FLAG_KEY_PRESSURE)
                    JNG_KEYP(jng_connection_data->tmp.state_ex.state, jng_connection_data->tmp.event.what) = jng_connection_data->tmp.event.value;
                else
                    JNG_KEYP(jng_connection_data->tmp.state_ex.state, jng_connection_data->tmp.event.what) = (jng_connection_data->tmp.event.value)? 255 : 0;
                
                if(jng_connection_data->tmp.event.value)
                    jng_connection_data->tmp.state_ex.state.keys |= jng_connection_data->tmp.event.what;
                else
                    jng_connection_data->tmp.state_ex.state.keys &= ~jng_connection_data->tmp.event.what;
                break;
            
            case JNG_EV_AXIS:
                jng_connection_data->tmp.event.what &= __JNG_AXIS_MASK;
                JNG_AXIS(jng_connection_data->tmp.state_ex.state, jng_connection_data->tmp.event.what) = jng_connection_data->tmp.event.value;
                break;
            
            case JNG_EV_SENSOR:
                jng_connection_data->tmp.event.what &= __JNG_SEN_MASK;
                JNG_SENSOR(jng_connection_data->tmp.state_ex.state, jng_connection_data->tmp.event.what) = jng_connection_data->tmp.event.value;
                break;
        }
        jng_state_wlock(js);
            memcpy(&js->state_ex.state, &jng_connection_data->tmp.state_ex.state, sizeof(jng_state_t));
            js->state_inc++;
        jng_state_wunlock(js);
        
        wake_up_interruptible(&js->state_queue);
        
        return sizeof(jng_event_t);
    }
    // Mod a blocchi
    if(len < sizeof(jng_state_t)) return -EINVAL;

    // stesso problema di read, copy_from_user può attendere
    // quindi la procedura è user->tmp --> wlock --> tmp->joystick --> wunlock
    if(copy_from_user(&jng_connection_data->tmp.state_ex.state, buffer, sizeof(jng_state_t)) != 0) return -EFAULT;

    jng_state_wlock(js);
        memcpy(&js->state_ex.state, &jng_connection_data->tmp.state_ex.state, sizeof(jng_state_t));
        js->state_inc++;
    jng_state_wunlock(js);

    wake_up_interruptible(&js->state_queue);
    return sizeof(jng_state_t);
}

static unsigned int jng_driver_poll(struct file* fp, poll_table* pt){
    unsigned int mask;
    jng_joystick_t* js;
    
    // Crea la copia locale
    jng_control_copy_joystick(js);
    
    // Se il driver blocca o no in lettura dipende dalla modalità
    // in scrittura invece non blocca mai
    mask = POLLOUT | POLLWRNORM;
    
    // Aggiungi comunque la coda eventi
    poll_wait(fp, &js->feedback_queue, pt);
    
    // Controlla la mod di lettura
    // In mod eventi se c'è almeno un evento in coda ritorna "leggibile"
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // In modalità eventi fa gli stessi controlli di read()
        // e ritorna POLLIN solo se c'è almeno un evento
        
        if(jng_update_driver_incremental(jng_connection_data, js) == 0){
            jng_gen_driver_events(jng_connection_data);
        }
        
        if(jng_list_len(&jng_connection_data->rbuffer) != 0){
            mask |= POLLIN | POLLRDNORM;
        }
    } else {
        // In mod a blocchi la lettura non blocca mai
        mask |= POLLIN | POLLRDNORM;
    }
    
    return mask;
}

static int jng_driver_fsync(struct file* fp, loff_t start, loff_t end, int datasync){
    // Non viene controllata la modalità in lettura
    // Se si è in lettura normale questo non avrà effetto
    jng_joystick_t* js;
    jng_control_copy_joystick(js);
    
    // Cancella la coda degli eventi
    jng_list_delall(&jng_connection_data->rbuffer);
    
    // Aggiorna le differenze. Non è necessario controllare js, dato che viene assegnato dal kernel in open()
    jng_feedback_lock(js);
        memcpy(&jng_connection_data->tmp.feedback_ex, &js->feedback_ex, sizeof(jng_feedback_ex_t));
    jng_feedback_unlock(js);
    
    memcpy(&jng_connection_data->diff.feedback_ex, &jng_connection_data->tmp.feedback_ex, sizeof(jng_feedback_ex_t));
    return 0;
}

static int jng_del_unwanted_events_cb(void* el, void* arg){
    return !(*((unsigned int*)arg) & ((jng_event_t*)el)->type);
}

static long jng_driver_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
    int rc;
    jng_joystick_t* js;
    jng_control_copy_joystick(js);
    
    printi("ioctl driver %d: %d(%ld)", js->num, cmd, arg);
    switch(cmd){
        case JNGIOCSETSLOT: // Solo client
            return -EINVAL;
        
        case JNGIOCGETSLOT:
            return copy_to_user((unsigned int*)arg, &js->num, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETINFO:
            jng_state_wlock(js);
                rc = copy_from_user(&js->info, (jng_info_t*)arg, sizeof(jng_info_t));
                js->info.connected = 1;
                
                // JNG_CTRL_INFO_CHANGED
                js->state_ex.control.last_info_inc = ++js->state_inc;
            jng_state_wunlock(js);
            
            return rc ? -EFAULT : 0;
        
        case JNGIOCGETINFO: // Perchè un driver dovrebbe volere le sue info? mah
            jng_state_rlock(js);
                rc = copy_to_user((jng_info_t*)arg, &js->info, sizeof(jng_info_t));
            jng_state_runlock(js);
            
            return rc ?-EFAULT : 0;
        
        case JNGIOCSETMODE:
            if(arg & JNG_RMODE_AGGREGATE) return -EINVAL; // Solo client
            
            jng_connection_data->mode = arg;
            if(!(arg & JNG_RMODE_EVENT)){
                // Quando si esce dalla lettura ad eventi cancella la coda
                jng_list_delall(&jng_connection_data->rbuffer);
            } else {
                // Se si imposta la lettura ad eventi si azzera la struttura delle differenze, così gli eventi vengono ricreati
                memset(&jng_connection_data->diff.feedback_ex, 0, sizeof(jng_feedback_ex_t));
            }
            return 0;
        
        case JNGIOCGETMODE:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->mode, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETEVMASK:
            jng_connection_data->evmask = arg;
            // Cancella eventuali eventi a cui il driver non è più interessato
            jng_list_delcb(&jng_connection_data->rbuffer, jng_del_unwanted_events_cb, &arg);
            
            return 0;
        
        case JNGIOCGETEVMASK:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->evmask, sizeof(unsigned int)) ? -EFAULT : 0;
        
        case JNGIOCAGRADD: // Solo client
            return -EINVAL;
        
        case JNGIOCAGRDEL: // Solo client
            return -EINVAL;
    }
    return -ENOTTY;
}

static int jng_driver_release(struct inode* in, struct file* fp){
    jng_joystick_t* js;
    
    jng_control_wlock();
    spin_lock(&jng_joysticks_lock);
        js->driver = NULL;
        memset(&js->state_ex.state, 0, sizeof(jng_state_t));
    spin_unlock(&jng_joysticks_lock);
    jng_control_wunlock();
    
    // JNG_CTRL_CONNECTION
    jng_state_wlock(js);
        js->info.connected = 0;
        
        js->state_ex.control.connected = 0;
        js->state_inc++;
    jng_state_wunlock(js);
    
    wake_up_interruptible(&js->state_queue); // Risveglia i client in ascolto
    
    kfree(fp->private_data);
    
    printi("Connessione con driver per il joystick %d chiusa", js->num);
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

