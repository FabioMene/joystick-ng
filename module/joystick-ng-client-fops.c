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

// Device driver per client (il file è una brutta copia di joystick-ng-driver-fops.c)

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
    jng_connection_data->mode     = JNG_MODE_BLOCK;
    jng_connection_data->evmask   = JNG_EV_CTRL | JNG_EV_KEY | JNG_EV_AXIS; // Di default non riceve gli eventi dei sensori
    
    jng_queue_init(&jng_connection_data->rbuffer, sizeof(jng_event_ex_t), JNG_MAX_CONN_EV);
    jng_connection_data->r_inc = 0;
    
    return 0;

  open__fail:
    return -_error;
    
    #undef _open_fail
}

// Controlla se i dati sono cambiati dall'ultima lettura. Se sì, aggiorna
// il contatore incrementale (conn->r_inc), copia lo stato del joystick in conn->tmpstate
// e ritorna 0 (atomicità con spinlock di lettura)
// Se i data non sono cambiati ritorna -1
static int jng_update_client_incremental(jng_connection_t* conn){
    int return_val = -1;
    
    jng_state_rlock(conn->joystick);
    if(conn->r_inc != conn->joystick->state_inc){
        memcpy(&conn->tmp.state_ex, &conn->joystick->state_ex, sizeof(jng_state_ex_t));
        return_val = 0;
        conn->r_inc = conn->joystick->state_inc;
    }
    jng_state_runlock(conn->joystick);
    
    return return_val;
}

// Aggiunge un evento sulla coda di lettura
static inline void jng_add_client_event(jng_connection_t* conn, unsigned short type, unsigned int what, int val){
    conn->tmp.event_ex.type  = type;
    conn->tmp.event_ex.what  = what;
    conn->tmp.event_ex.value = val;
    jng_queue_add(&conn->rbuffer, &conn->tmp.event_ex);
}

// Trova le differenze tra conn->tmp.state_ex e conn->diff.state_ex e genera gli eventi
// rispettando la event mask
// Questa funzione richiede che conn->tmp.event_ex.num sia impostato

static void jng_gen_client_events(jng_connection_t* conn){
    // Questa macro evita 5000000 milioni di if
    #define jng_diff_cmp(_prop, _type, _what) if(conn->diff.state_ex._prop != conn->tmp.state_ex._prop) jng_add_client_event(conn, _type, _what, conn->tmp.state_ex._prop)
    
    // Eventi di controllo
    if(conn->evmask & JNG_EV_CTRL){
        jng_diff_cmp(control.connected,     JNG_EV_CTRL, JNG_CTRL_CONNECTION);
        jng_diff_cmp(control.last_info_inc, JNG_EV_CTRL, JNG_CTRL_INFO_CHANGED);
    }
    
    // Tasti
    if(conn->evmask & JNG_EV_KEY){
        jng_diff_cmp(state.keyp.A,        JNG_EV_KEY, JNG_KEY_A);
        jng_diff_cmp(state.keyp.B,        JNG_EV_KEY, JNG_KEY_B);
        jng_diff_cmp(state.keyp.C,        JNG_EV_KEY, JNG_KEY_C);
        jng_diff_cmp(state.keyp.X,        JNG_EV_KEY, JNG_KEY_X);
        jng_diff_cmp(state.keyp.Y,        JNG_EV_KEY, JNG_KEY_Y);
        jng_diff_cmp(state.keyp.Z,        JNG_EV_KEY, JNG_KEY_Z);
        jng_diff_cmp(state.keyp.L1,       JNG_EV_KEY, JNG_KEY_L1);
        jng_diff_cmp(state.keyp.R1,       JNG_EV_KEY, JNG_KEY_R1);
        jng_diff_cmp(state.keyp.L2,       JNG_EV_KEY, JNG_KEY_L2);
        jng_diff_cmp(state.keyp.R2,       JNG_EV_KEY, JNG_KEY_R2);
        jng_diff_cmp(state.keyp.L3,       JNG_EV_KEY, JNG_KEY_L3);
        jng_diff_cmp(state.keyp.R3,       JNG_EV_KEY, JNG_KEY_R3);
        jng_diff_cmp(state.keyp.L1,       JNG_EV_KEY, JNG_KEY_L1);
        jng_diff_cmp(state.keyp.R1,       JNG_EV_KEY, JNG_KEY_R1);
        jng_diff_cmp(state.keyp.Down,     JNG_EV_KEY, JNG_KEY_DOWN);
        jng_diff_cmp(state.keyp.Right,    JNG_EV_KEY, JNG_KEY_RIGHT);
        jng_diff_cmp(state.keyp.Left,     JNG_EV_KEY, JNG_KEY_LEFT);
        jng_diff_cmp(state.keyp.Up,       JNG_EV_KEY, JNG_KEY_UP);
        jng_diff_cmp(state.keyp.Start,    JNG_EV_KEY, JNG_KEY_START);
        jng_diff_cmp(state.keyp.Select,   JNG_EV_KEY, JNG_KEY_SELECT);
        jng_diff_cmp(state.keyp.Options1, JNG_EV_KEY, JNG_KEY_OPTIONS1);
        jng_diff_cmp(state.keyp.Options2, JNG_EV_KEY, JNG_KEY_OPTIONS2);
        jng_diff_cmp(state.keyp.Other1,   JNG_EV_KEY, JNG_KEY_OTHER1);
        jng_diff_cmp(state.keyp.Other2,   JNG_EV_KEY, JNG_KEY_OTHER2);
    }
    
    // Assi
    if(conn->evmask & JNG_EV_AXIS){
        jng_diff_cmp(state.axis.LX, JNG_EV_AXIS, JNG_AXIS_LX);
        jng_diff_cmp(state.axis.LY, JNG_EV_AXIS, JNG_AXIS_LY);
        jng_diff_cmp(state.axis.RX, JNG_EV_AXIS, JNG_AXIS_RX);
        jng_diff_cmp(state.axis.RY, JNG_EV_AXIS, JNG_AXIS_RY);
        jng_diff_cmp(state.axis.G1, JNG_EV_AXIS, JNG_AXIS_G1);
        jng_diff_cmp(state.axis.G2, JNG_EV_AXIS, JNG_AXIS_G2);
        jng_diff_cmp(state.axis.G3, JNG_EV_AXIS, JNG_AXIS_G3);
        jng_diff_cmp(state.axis.G4, JNG_EV_AXIS, JNG_AXIS_G4);
    }
    
    // Sensori
    if(conn->evmask & JNG_EV_SENSOR){
        jng_diff_cmp(state.accelerometer.x, JNG_EV_SENSOR, JNG_SEN_ACCEL_X);
        jng_diff_cmp(state.accelerometer.y, JNG_EV_SENSOR, JNG_SEN_ACCEL_Y);
        jng_diff_cmp(state.accelerometer.z, JNG_EV_SENSOR, JNG_SEN_ACCEL_Z);
        jng_diff_cmp(state.gyrometer.x,     JNG_EV_SENSOR, JNG_SEN_GYRO_X);
        jng_diff_cmp(state.gyrometer.y,     JNG_EV_SENSOR, JNG_SEN_GYRO_Y);
        jng_diff_cmp(state.gyrometer.z,     JNG_EV_SENSOR, JNG_SEN_GYRO_Z);
    }
    
    #undef jng_diff_cmp
    
    // Aggiornamento diff
    memcpy(&conn->diff.state_ex, &conn->tmp.state_ex, sizeof(jng_state_ex_t));
}

static ssize_t jng_client_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->joystick == NULL) return -EINVAL;
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // Modalità eventi
        
        if(len < sizeof(jng_event_t)) return -EINVAL;
        
        if(len >= sizeof(jng_event_ex_t)) len = sizeof(jng_event_ex_t);
        else                              len = sizeof(jng_event_t);
        
      jng_cl_rd_read_again:
        // Controlla se lo stato è cambiato dall'ultimo controllo, e se sì,
        // genera eventuali nuovi eventi
        if(jng_update_client_incremental(jng_connection_data) == 0){
            jng_connection_data->tmp.event_ex.num = jng_connection_data->joystick->num;
            jng_gen_client_events(jng_connection_data);
        }
        
        // È possibile che non ci sia nessun evento
        if(jng_queue_pop(&jng_connection_data->rbuffer, &jng_connection_data->tmp.event_ex)){
            if(fp->f_flags & O_NONBLOCK) return -EAGAIN;
            // Dobbiamo aspettare
            if(wait_event_interruptible(jng_connection_data->joystick->state_queue, jng_connection_data->r_inc != jng_connection_data->joystick->state_inc)) return -ERESTARTSYS;
            // Controlliamo di nuovo i dati
            goto jng_cl_rd_read_again;
        }
        
        // Copiamo l'evento in user space
        return copy_to_user(buffer, &jng_connection_data->tmp.event_ex, len) ? -EFAULT : len;
    }
    
    // Mod normale
    if(len < sizeof(jng_state_t)) return -EINVAL;
    
    if(len >= sizeof(jng_state_ex_t)) len = sizeof(jng_state_ex_t);
    else                              len = sizeof(jng_state_t);
    
    // copy_to_user può andare in sleep
    jng_state_rlock(jng_connection_data->joystick);
        memcpy(&jng_connection_data->tmp.state_ex, &jng_connection_data->joystick->state_ex, sizeof(jng_state_ex_t));
    jng_state_runlock(jng_connection_data->joystick);
    
    return copy_to_user(buffer, &jng_connection_data->tmp.state_ex, len) ? -EFAULT : len;
}

static ssize_t jng_client_write(struct file* fp, const char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->mode & JNG_WMODE_EVENT){
        jng_joystick_t* js;
        
        if(len < sizeof(jng_event_t)) return -EINVAL;
        
        if(len >= sizeof(jng_event_ex_t)){
            len = sizeof(jng_event_ex_t);
        } else {
            if(jng_connection_data->joystick == NULL) return -EINVAL;
            
            len = sizeof(jng_event_t);
            jng_connection_data->tmp.event_ex.num = jng_connection_data->joystick->num;
        }
        
        if(copy_from_user(&jng_connection_data->tmp.event_ex, buffer, len) != 0) return -EFAULT;
        
        if(jng_connection_data->tmp.event_ex.num < 0 || jng_connection_data->tmp.event_ex.num > JNG_TOT - 1) return -EINVAL;
        
        js = jng_joysticks + jng_connection_data->tmp.event_ex.num;
        
        // Più joystick possono essere collegati allo stesso joystick, quindi la strategia adottata in jng_driver_write
        // non può essere utilizzata. Qui il blocco è totale
        
        jng_feedback_lock(js);
            switch(jng_connection_data->tmp.event_ex.type){
                case JNG_EV_FB_FORCE:
                    jng_connection_data->tmp.event.what &= __JNG_FB_FORCE_MASK;
                    JNG_FB_FORCE(js->feedback_ex.feedback, jng_connection_data->tmp.event.what) = jng_connection_data->tmp.event.value;
                    break;
                
                case JNG_EV_FB_LED:
                    jng_connection_data->tmp.event.what &= __JNG_FB_LED_MASK;
                    JNG_FB_LED(js->feedback_ex.feedback, jng_connection_data->tmp.event.what) = jng_connection_data->tmp.event.value;
                    break;
            }
            js->feedback_inc++;
        jng_feedback_unlock(js);
        
        // Risveglia il driver se serve
        wake_up_interruptible(&js->feedback_queue);
        
        // fine
        return len;
    }
    
    if(jng_connection_data->joystick == NULL) return -EINVAL;
    
    // Mod normale
    if(len < sizeof(jng_feedback_t)) return -EINVAL;
    
    // copy_from_user può attendere
    // quindi la procedura è user->tmp --> lock --> tmp->joystick --> unlock
    if(copy_from_user(&jng_connection_data->tmp.feedback_ex.feedback, buffer, sizeof(jng_feedback_t)) != 0) return -EFAULT;

    jng_feedback_lock(jng_connection_data->joystick);
        memcpy(&jng_connection_data->joystick->feedback_ex.feedback, &jng_connection_data->tmp.feedback_ex.feedback, sizeof(jng_feedback_t));
        jng_connection_data->joystick->feedback_inc++;
    jng_feedback_unlock(jng_connection_data->joystick);

    wake_up_interruptible(&jng_connection_data->joystick->feedback_queue);

    return sizeof(jng_feedback_t);
}

static unsigned int jng_client_poll(struct file* fp, poll_table* pt){
    unsigned int mask;
    
    if(jng_connection_data->joystick == NULL) return -EINVAL;
    
    // In scrittura non blocca mai
    mask = POLLOUT | POLLWRNORM;
    
    // Aggiungi comunque la coda eventi
    // TODO: Capire cosa intendevo con questo commento
    poll_wait(fp, &jng_connection_data->joystick->state_queue, pt);
    
    // Controlla la mod di lettura
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // In modalità eventi fa gli stessi controlli di read()
        // e ritorna POLLIN solo se c'è almeno un evento
        
        if(jng_update_client_incremental(jng_connection_data) == 0){
            jng_gen_client_events(jng_connection_data);
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

static int jng_client_fsync(struct file* fp, loff_t start, loff_t end, int datasync){
    // Non viene controllata la modalità in lettura
    // Se si è in lettura normale questo non avrà effetto
    
    // Cancella la coda degli eventi
    jng_queue_delall(&jng_connection_data->rbuffer);
    
    // Aggiorna le differenze, solo se lo slot è stato impostato
    if(jng_connection_data->joystick != NULL){
        jng_state_rlock(jng_connection_data->joystick);
            memcpy(&jng_connection_data->tmp.state_ex, &jng_connection_data->joystick->state_ex, sizeof(jng_state_ex_t));
        jng_state_runlock(jng_connection_data->joystick);
        
        memcpy(&jng_connection_data->diff.state_ex, &jng_connection_data->tmp.state_ex, sizeof(jng_state_ex_t));
    }
    
    return 0;
}

static int jng_del_unwanted_events_cb(void* el, void* arg){
    return !(*((unsigned int*)arg) & ((jng_event_ex_t*)el)->type);
}

static long jng_client_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
    int rc;
    jng_info_t tmpinfo;
    
    // Evita di spammare messaggi di log per la selezione joystick, dato che tecnicamente
    // si possono leggere i dati di tutti e 32 i joystick da un solo fd
    if(cmd != JNGIOCSETSLOT) printi("ioctl client: %d(%ld)", cmd, arg);
    
    switch(cmd){
        case JNGIOCSETSLOT:
            if(arg < 0 || arg > JNG_TOT - 1) return -EINVAL; // Sta cercando di fotterci questo
            jng_connection_data->joystick = jng_joysticks + arg;
            // Permetti la rigenerazione di tutti gli eventi cancellando le differenze
            memset(&jng_connection_data->diff.state_ex, 0, sizeof(jng_state_ex_t));
            return 0;
        
        case JNGIOCGETSLOT:
            if(jng_connection_data->joystick == NULL) return -EINVAL;
            return copy_to_user((unsigned int*)arg, &jng_connection_data->joystick->num, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETINFO: // Solo driver
            return -EINVAL;
        
        case JNGIOCGETINFO:
            if(jng_connection_data->joystick == NULL) return -EINVAL;
            
            jng_state_wlock(jng_connection_data->joystick);
                memcpy(&tmpinfo, &jng_connection_data->joystick->info, sizeof(jng_info_t));
                
                // Aggiorna l'ultima volta che le info sono state controllate
                jng_connection_data->diff.state_ex.control.last_info_inc = jng_connection_data->joystick->state_ex.control.last_info_inc;
            jng_state_wunlock(jng_connection_data->joystick);
            
            rc = copy_to_user((jng_info_t*)arg, &tmpinfo, sizeof(jng_info_t));
            return rc ?-EFAULT : 0;
        
        
        case JNGIOCSETMODE:
            if(jng_connection_data->joystick == NULL) return -EINVAL;
            jng_connection_data->mode = arg;
            
            if(!(arg & JNG_RMODE_EVENT)){
                // Cancella la coda degli eventi quando si va in mod lettura normale
                jng_queue_delall(&jng_connection_data->rbuffer);
            } else {
                // Azzera le differenze quando si entra in mod lettura eventi
                memset(&jng_connection_data->diff.state_ex, 0, sizeof(jng_state_ex_t));
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
    .poll           = jng_client_poll,
    .fsync          = jng_client_fsync,
    .write          = jng_client_write,
    
    .unlocked_ioctl = jng_client_ioctl,
    .compat_ioctl   = jng_client_ioctl,
    
    .release        = jng_client_release,
    
    .llseek         = no_llseek
};
