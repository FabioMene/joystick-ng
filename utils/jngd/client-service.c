/*
 * client-service.c
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

#include "jngd.h"

void* client_service(void* arg){
    int  sock = ((jngd_thread_arg_t*)arg)->fd;
    int* zoe  = ((jngd_thread_arg_t*)arg)->zero_on_exit;
    free(arg);
    
    // Inizio thread
    
    int is_elev = isUserElevated(sock);
    
    printi("fd: %d | isUserElevated(): %s", sock, (is_elev < 0)?"Errore":(is_elev)?"sì":"no");
    
    unsigned char buffer[65536];
    
    int len, ret;
    
    while(1){
        len = read(sock, buffer, 65535);
        
        if(len < 0){
            printw("fd: %d | Errore ricezione dati", sock);
            break;
        }
        
        if(len == 0) break; // Fine normale
        
        if(len < 1){
            printw("fd: %d | Errore protocollo", sock);
            break;
        }
        
        len--;
        switch((jngd_action_e)buffer[0]){
            #define CHECK_ELEVATION() if(is_elev != 1){printi("fd: %d | Permesso negato", sock);ret = EACCES;break;}
            
            case JNGD_ACTION_DRV_LAUNCH:
                printi("fd: %d | Nuova richiesta: [DRV_LAUNCH]", sock);
                CHECK_ELEVATION();
                
                ret = do_drv_launch(buffer + 1, &len);
                break;
            
            case JNGD_ACTION_DRV_LIST:
                printi("fd: %d | Nuova richiesta: [DRV_LIST]", sock);
                ret = do_drv_list(buffer + 1, &len);
                break;
            
            case JNGD_ACTION_DRVOPT_UPDATE:
                printi("fd: %d | Nuova richiesta: [DRVOPT_UPDATE]", sock);
                CHECK_ELEVATION();
                
                ret = do_drvopt_update(buffer + 1, &len);
                break;
            
            case JNGD_ACTION_DRVOPT_LIST:
                printi("fd: %d | Nuova richiesta: [DRVOPT_LIST]", sock);
                ret = do_drvopt_list(buffer + 1, &len);
                break;
            
            case JNGD_ACTION_DRVOPT_GET:
                printi("fd: %d | Nuova richiesta: [DRVOPT_GET]", sock);
                ret = do_drvopt_get(buffer + 1, &len);
                break;
            
            case JNGD_ACTION_DRVOPT_SET:
                printi("fd: %d | Nuova richiesta: [DRVOPT_SET]", sock);
                CHECK_ELEVATION();
                
                ret = do_drvopt_set(buffer + 1, &len);
                break;
            
            default:
                printw("fd: %d | Azione non valida: %02x", sock, buffer[0]);
                ret = EINVAL;
                break;
        }
        len++;
        
        // Stati di errore
        if(ret != 0){
            buffer[0] = ret;
            len = 1;
        } else {
            buffer[0] = 0;
        }
        
        if(write(sock, buffer, len) != len){
            printw("fd: %d | Impossibile inviare la risposta al client", sock);
        }
    }
    
    printi("fd: %d | Uscita", sock);
    
    // Fine thread
    
    close(sock);
    
    pthread_mutex_lock(&threads_mutex);
        *zoe = 0;
    pthread_mutex_unlock(&threads_mutex);
    return NULL;
}
