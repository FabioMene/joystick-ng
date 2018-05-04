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

#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>

#include "jngd.h"

void* client_service(void* arg){
    int  sock = ((jngd_thread_arg_t*)arg)->fd;
    int* zoe  = ((jngd_thread_arg_t*)arg)->zero_on_exit;
    free(arg);
    
    // Inizio thread
    
    int is_elev = isUserElevated(sock);
    
    unsigned char buffer[65536];
    
    int len, ret;
    
    while(1){
        len = read(sock, buffer, 65535);
        
        if(len < 0) break;
        
        if(len < 1){
            printw("Errore protocollo");
            break;
        }
        
        len--;
        switch(buffer[0]){
            case JNGD_ACTION_DRV_LAUNCH:
                if(is_elev){
                    ret = do_drv_launch(buffer + 1, &len);
                } else {
                    ret = EACCESS;
                }
                break;
            
            case JNGD_ACTION_DRV_LIST:
                ret = do_drv_list(buffer + 1, &len);
                break;
            
            default:
                printw("Azione non valida: %02x", buffer[0]);
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
        
        if(send(sock, buffer, len + 1) != len){
            printw("Impossibile inviare la risposta al client");
        }
    }
    
    
    /*
    
    unsigned char* packet = buffer + 1;
    
    // Mainloop
    while(1){
        int len;
        
      _skip_to_recv:
        len = recvfrom(servfd, buffer, 32768, 0, NULL, NULL);
        if(len < 0){
            printe("Errore ricezione pacchetto (errno %d)", errno);
            close(servfd);
            unlink(SOCKET_FILE);
            return 1;
        }
        
        if(len < 1){
            printw("Pacchetto senza header (ed è di un solo byte)");
            continue;
        }
        
        len--;
        
        #define check_size(min) {int __min = (min); if(len < __min){printw("Lunghezza pacchetto richiesta: %d, ricevuta: %d", __min, len);goto _skip_to_recv;}}
        
        switch(buffer[0]){
            case ACTION_DRV_LAUNCH: {
                
            } break;
            
            case ACTION_DRV_MODSETT: {
                // Controllo pacchetti e sanity check
                check_size(3);
                check_size(3 + packet[0] + packet[1] + packet[2]);
                
                packet[3 + packet[0] - 1] = 0;
                packet[3 + packet[0] + packet[1] - 1] = 0;
                packet[3 + packet[0] + packet[1] + packet[2] - 1] = 0;
                
                // Carica il driver
                if(jngdsett_load((char*)packet + 3) < 0){
                    printe("[DRV_MODSETT] Driver \"%s\" non trovato", packet + 3);
                    goto _skip_to_recv;
                }
                
                int res = jngdsett_write((char*)packet + 3 + packet[0], (char*)packet + 3 + packet[0] + packet[1]);
                
                jngdsett_reset();
                
                if(res){
                    printe("[DRV_MODSETT] Scrittura fallita per il driver %s", packet + 3);
                }
            } break;
        }
        
        #undef check_size
    }*/
    
    // Fine thread
    
    close(sock);
    
    pthread_mutex_lock(&threads_mutex);
        *zoe = 0;
    pthread_mutex_unlock(&threads_mutex);
    return NULL;
}
