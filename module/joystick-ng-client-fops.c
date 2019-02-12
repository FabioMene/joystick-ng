/*
 * joystick-ng-client-fops.c
 * 
 * Copyright 2015-2019 Fabio Meneghetti <fabiomene97@gmail.com>
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
#include <linux/sched/signal.h>
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
    
    jng_list_init(&jng_connection_data->rbuffer, sizeof(jng_event_ex_t), JNG_MAX_CONN_EV);
    jng_connection_data->r_inc = 0;
    
    jng_list_init(&jng_connection_data->client_data.aggregate, sizeof(jng_aggregate_element_t), JNG_TOT);
    
    return 0;

  open__fail:
    return -_error;
    
    #undef _open_fail
}

// Controlla se i dati sono cambiati dall'ultima lettura. Se sì, aggiorna
// il contatore incrementale (inc), copia lo stato del joystick in state
// e ritorna 0 (atomicità con spinlock di lettura)
// Se i data non sono cambiati ritorna -1
static int jng_update_client_incremental(jng_joystick_t* js, jng_state_ex_t* state, unsigned int* inc){
    int return_val = -1;
    
    jng_state_rlock(js);
        if(*inc != js->state_inc){
            memcpy(state, &js->state_ex, sizeof(jng_state_ex_t));
            return_val = 0;
            *inc = js->state_inc;
        }
    jng_state_runlock(js);
    
    return return_val;
}

// Aggiunge un evento sulla coda di lettura
static inline void jng_add_client_event(jng_connection_t* conn, unsigned short type, unsigned int what, int val){
    conn->tmp.event_ex.type  = type;
    conn->tmp.event_ex.what  = what;
    conn->tmp.event_ex.value = val;
    jng_list_append(&conn->rbuffer, &conn->tmp.event_ex);
}

// Trova le differenze tra conn->tmp.state_ex e diff e genera gli eventi
// rispettando la event mask
// Questa funzione richiede che conn->tmp.event_ex.num sia impostato

static void jng_gen_client_events(jng_connection_t* conn, jng_state_ex_t* diff){
    // Questa macro evita 5000000 milioni di if
    #define jng_diff_cmp(_prop, _type, _what) if(diff->_prop != conn->tmp.state_ex._prop) jng_add_client_event(conn, _type, _what, conn->tmp.state_ex._prop)
    
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
    memcpy(diff, &conn->tmp.state_ex, sizeof(jng_state_ex_t));
}

static void jng_gen_aggregate_events_cb(void* el, void* arg){
    jng_connection_t*        conn = (jng_connection_t*)arg;
    jng_aggregate_element_t* ael  = (jng_aggregate_element_t*)el;
    
    if(jng_update_client_incremental(ael->js, &conn->tmp.state_ex, &ael->inc) == 0){
        // Genera eventi
        conn->tmp.event_ex.num = ael->js->num;
        jng_gen_client_events(conn, &ael->diff);
    }
}

// Controlla la modalità di lettura e genera gli eventi per questo client
static int jng_update_client_events(jng_connection_t* conn){
    // Dividi tra modalità aggregata e non
    if(conn->mode & JNG_RMODE_AGGREGATE){
        // Itera con callback la lista client_data.aggregate
        jng_list_iter(&conn->client_data.aggregate, jng_gen_aggregate_events_cb, conn);
    } else {
        // Sanity check
        if(conn->joystick == NULL) return -1;
        
        // Controlla se lo stato è cambiato dall'ultimo controllo, e se sì,
        // genera eventuali nuovi eventi
        if(jng_update_client_incremental(conn->joystick, &conn->tmp.state_ex, &conn->r_inc) == 0){
            // Genera eventi
            conn->tmp.event_ex.num = conn->joystick->num;
            jng_gen_client_events(conn, &conn->diff.state_ex);
        }
    }
    
    return 0;
}

typedef struct {
    jng_joystick_t* joysticks[JNG_TOT];
    uint32_t        incs[JNG_TOT];
    uint32_t        num;
} jng_iter_wait_argument_t;

static void jng_fill_wait_argument_cb(void* el, void* arg){
    jng_aggregate_element_t*  ael = (jng_aggregate_element_t*)el;
    jng_iter_wait_argument_t* wa  = (jng_iter_wait_argument_t*)arg;
    
    
    wa->joysticks[wa->num] = ael->js;
    wa->incs[wa->num]      = ael->inc;
    wa->num++;
}

// wake_up_interruptible, ma per code multiple (mod. aggregata)
static int jng_wait_mult_event_interruptible(jng_connection_t* conn){
    jng_iter_wait_argument_t was = {
        .num = 0
    };
    uint32_t i;
    int ret, break_while;
    wait_queue_entry_t* queues;
    
    jng_list_iter(&conn->client_data.aggregate, jng_fill_wait_argument_cb, &was);
    
    queues = kmalloc(sizeof(wait_queue_entry_t) * was.num, GFP_KERNEL);
    if(!queues) return -ENOMEM;
    
    for(i = 0;i < was.num;i++){
        // Inizializza l'elemento
        init_wait(queues + i);
        queues[i].func = default_wake_function;
        
        // Aggiungi alla lista
        add_wait_queue(&was.joysticks[i]->state_queue, queues + i);
    }
    
    ret = 0;
    
    // Parte critica (penso)
    while(1){
        //
        set_current_state(TASK_INTERRUPTIBLE);
        
        // Controlla tutte le condizioni di uscita
        break_while = 0;
        for(i = 0;i < was.num;i++){
            if(was.incs[i] != was.joysticks[i]->state_inc){
                break_while = 1;
                break;
            }
        }
        
        if(break_while) break;
        
        // Niente di nuovo, schedula il processo
        schedule();
        
        // Controlla segnali in attesa
        if(signal_pending(current)){
            ret = -ERESTARTSYS;
            break;
        }
    }
    
    // Rimuovi gli elementi dalle code
    for(i = 0;i < was.num;i++){
        remove_wait_queue(&was.joysticks[i]->state_queue, queues + i);
    }
    
    set_current_state(TASK_RUNNING);
    // Fine parte critica
    
    kfree(queues);
    
    return ret;
}

static ssize_t jng_client_read(struct file* fp, char __user* buffer, size_t len, loff_t* offp){
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // Modalità eventi
        if(len < sizeof(jng_event_t)) return -EINVAL;
        
        if(len >= sizeof(jng_event_ex_t)) len = sizeof(jng_event_ex_t);
        else                              len = sizeof(jng_event_t);
        
      jng_cl_rd_read_again:
        
        if(jng_update_client_events(jng_connection_data)) return -EINVAL;
        
        // È possibile che non ci sia nessun evento
        if(jng_list_pop(&jng_connection_data->rbuffer, &jng_connection_data->tmp.event_ex)){
            int ret = 0;
            
            if(fp->f_flags & O_NONBLOCK) return -EAGAIN;
            // Dobbiamo aspettare
            if(jng_connection_data->mode & JNG_RMODE_AGGREGATE)
                ret = jng_wait_mult_event_interruptible(jng_connection_data);
            else
                ret = wait_event_interruptible(
                    jng_connection_data->joystick->state_queue,
                    jng_connection_data->r_inc != jng_connection_data->joystick->state_inc
                );
            
            if(ret) return ret;
            
            // Controlliamo di nuovo i dati
            goto jng_cl_rd_read_again;
        }
        
        // Copiamo l'evento in user space
        return copy_to_user(buffer, &jng_connection_data->tmp.event_ex, len) ? -EFAULT : len;
    }
    
    // Mod normale
    if(jng_connection_data->joystick == NULL) return -EINVAL;
    
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

typedef struct {
    struct file* fp;
    poll_table*  pt;
} jng_iter_poll_argument_t;

static void jng_add_to_poll_table_cb(void* el, void* arg){
    jng_iter_poll_argument_t* pa = (jng_iter_poll_argument_t*)arg;
    
    poll_wait(pa->fp, &((jng_aggregate_element_t*)el)->js->state_queue, pa->pt);
}

static unsigned int jng_client_poll(struct file* fp, poll_table* pt){
    unsigned int mask;
    
    // In scrittura non blocca mai
    mask = POLLOUT | POLLWRNORM;
    
    // A seconda della modalità di lettura aggiungi la coda 
    if(jng_connection_data->mode & JNG_RMODE_AGGREGATE){
        jng_iter_poll_argument_t pa = {
            .fp = fp,
            .pt = pt
        };
        
        jng_list_iter(&jng_connection_data->client_data.aggregate, jng_add_to_poll_table_cb, &pa);
    } else {
        poll_wait(fp, &jng_connection_data->joystick->state_queue, pt);
    }
    
    // Controlla la mod di lettura
    if(jng_connection_data->mode & JNG_RMODE_EVENT){
        // In modalità eventi fa gli stessi controlli di read()
        // e ritorna POLLIN solo se c'è almeno un evento
        
        jng_update_client_events(jng_connection_data);
        
        if(jng_list_len(&jng_connection_data->rbuffer) != 0){
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
    jng_list_delall(&jng_connection_data->rbuffer);
    
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
    return !(((unsigned long)arg) & ((jng_event_ex_t*)el)->type);
}

static int jng_del_unwanted_aggregate_cb(void* el, void* arg){
    return ((jng_aggregate_element_t*)el)->js->num == (unsigned long)arg;
}

static long jng_client_ioctl(struct file* fp, unsigned int cmd, unsigned long arg){
    int rc;
    jng_info_t tmpinfo;
    jng_aggregate_element_t tmpagr;
    
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
            // La modalità aggregata ignora jng_connection_data->joystick
        
            if((arg & JNG_RMODE_AGGREGATE) == 0 && jng_connection_data->joystick == NULL) return -EINVAL;
            jng_connection_data->mode = arg;
            
            if((arg & JNG_RMODE_EVENT) == 0){
                // Cancella la coda degli eventi quando si va in mod lettura normale
                jng_list_delall(&jng_connection_data->rbuffer);
            } else if((arg & JNG_RMODE_AGGREGATE) == 0){
                // Azzera le differenze quando si entra in mod lettura eventi non aggregati
                memset(&jng_connection_data->diff.state_ex, 0, sizeof(jng_state_ex_t));
                
                // Cancella le impostazioni di lettura aggregata
                jng_list_delall(&jng_connection_data->client_data.aggregate);
            }
            return 0;
        
        case JNGIOCGETMODE:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->mode, sizeof(unsigned int)) ? -EFAULT : 0;
        
        
        case JNGIOCSETEVMASK:
            jng_connection_data->evmask = arg;
            // Cancella eventuali eventi a cui il client non è più interessato
            jng_list_delcb(&jng_connection_data->rbuffer, jng_del_unwanted_events_cb, (void*)arg);
            return 0;
        
        case JNGIOCGETEVMASK:
            return copy_to_user((unsigned int*)arg, &jng_connection_data->evmask, sizeof(unsigned int)) ? -EFAULT : 0;
        
        case JNGIOCAGRADD:
            if((jng_connection_data->mode & JNG_RMODE_AGGREGATE) == 0) return -EINVAL;
            
            if(arg < 0 || arg > JNG_TOT - 1) return -EINVAL;
            
            // Rimuovi l'eventuale copia corrente
            jng_list_delcb(&jng_connection_data->client_data.aggregate, jng_del_unwanted_aggregate_cb, (void*)arg);
            
            // Crea il nuovo elemento della lista
            tmpagr.js = jng_joysticks + arg;
            
            memset(&tmpagr.diff, 0, sizeof(jng_state_ex_t));
            
            tmpagr.inc = 0;
            
            // Aggiungi l'elemento
            jng_list_append(&jng_connection_data->client_data.aggregate, &tmpagr);
            
            return 0;
        
        case JNGIOCAGRDEL:
            if((jng_connection_data->mode & JNG_RMODE_AGGREGATE) == 0) return -EINVAL;
            
            // Cancella dalla lista, se presente
            jng_list_delcb(&jng_connection_data->client_data.aggregate, jng_del_unwanted_aggregate_cb, (void*)arg);
            
            return 0;
        
        case JNGIOCDROPEVT:
            jng_update_client_events(jng_connection_data);
            
            arg = ~arg;
            jng_list_delcb(&jng_connection_data->rbuffer, jng_del_unwanted_events_cb, (void*)arg);
            
            return 0;
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
