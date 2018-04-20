/*
 * main.c
 * 
 * Copyright 2016-2017 Fabio Meneghetti <fabiomene97@gmail.com>
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


// Questo è un driver per joystick-ng per DualShock 3 - Bluetooth

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hidp.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include "../../include/joystick-ng.h"
#include "../../utils/libjngdsett/libjngdsett.h"


#define printd(fmt...) syslog(LOG_DEBUG,   fmt);
#define printi(fmt...) syslog(LOG_INFO,    fmt);
#define printw(fmt...) syslog(LOG_WARNING, fmt);
#define printe(fmt...) syslog(LOG_ERR,     fmt);


#define PID_FILE "/var/run/ds3bt.pid"
#define FEEDBACK_POLL_TIME_USEC 5000 // 5ms


#define L2CAP_PSM_HIDP_CTRL 0x11
#define L2CAP_PSM_HIDP_INTR 0x13

int l2cap_create_socket(const bdaddr_t *bdaddr, unsigned short psm, int lm, int backlog);
int get_sdp_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req);

int client_loop(int ctrl, int intr);
void* sender_thread_loop(void* arg);

jng_info_t jng_info = {
    .name      = "Sony DualShock3 (Bluetooth)",
    .keys      = JNG_KEY_ABXY | JNG_KEY_L1 | JNG_KEY_R1 | JNG_KEY_L2 | JNG_KEY_R2 | JNG_KEY_L3 | JNG_KEY_R3 | JNG_KEY_DIRECTIONAL | JNG_KEY_START | JNG_KEY_SELECT | JNG_KEY_OPTIONS1,
    .axis      = JNG_AXIS_LX | JNG_AXIS_LY | JNG_AXIS_RX | JNG_AXIS_RY,
    .sensors   = JNG_SEN_ACCEL_X | JNG_SEN_ACCEL_Y | JNG_SEN_ACCEL_Z | JNG_SEN_GYRO_X,
    .fb_force  = JNG_FB_FORCE_BIGMOTOR | JNG_FB_FORCE_SMALLMOTOR,
    .fb_led    = JNG_FB_LED_1 | JNG_FB_LED_2 | JNG_FB_LED_3 | JNG_FB_LED_4,
    .flags     = JNG_FLAG_KEY_PRESSURE,
    .keyp      = JNG_KEY_ABXY | JNG_KEY_L1 | JNG_KEY_R1 | JNG_KEY_L2 | JNG_KEY_R2 | JNG_KEY_DIRECTIONAL
};

typedef struct {
    unsigned char      type;
    unsigned char      res1;
    unsigned short int keys;
    unsigned char      ps;
    unsigned char      res2;
    unsigned char      LX;
    unsigned char      LY;
    unsigned char      RX;
    unsigned char      RY;
    unsigned char      res3[4];
    unsigned char      Up;
    unsigned char      Right;
    unsigned char      Down;
    unsigned char      Left;
    unsigned char      L2;
    unsigned char      R2;
    unsigned char      L1;
    unsigned char      R1;
    unsigned char      Y;
    unsigned char      B;
    unsigned char      A;
    unsigned char      X;
    unsigned char      res4[3];
    unsigned char      cstate; // non 3: in carica, 3: non in carica
    unsigned char      blevel; // 238: in carica (livello non disponibile), 0 -> 5: 0 -> 100%
    unsigned char      conn; // usb/bluetooth?
    unsigned char      res5[9];
    unsigned short int accelX;
    unsigned short int accelY;
    unsigned short int accelZ;
    unsigned short int gyroX;
} ds3_report_t;

typedef struct {
    int jngfd;
    int ctrlfd;
    
    unsigned char blevel;
} sender_thread_arg_t;

void server_sigterm_handler(int sig){
    unlink(PID_FILE);
    printw("SIGTERM, Uscita...");
    exit(0);
}

void client_sigterm_handler(int sig){
    _Exit(0);
}

int main(int argc, char* argv[]){
    // Apri log
    openlog("jngd/ds3bt", LOG_CONS | LOG_PID, LOG_DAEMON);
    printi("Init");
    
    jngdsett_load(NULL);
    
    int autostart = 0;
    jngdsett_read("autostart", &autostart);
    
    int fd;
    
    if(argc > 1){
        if(strcmp(argv[1], "autostart") == 0 && autostart == 0){
            printi("Autostart ignorato (da impostazioni)");
            return 0;
        }
        // Ferma il server
        if(strcmp(argv[1], "stop") == 0){
            fd = open(PID_FILE, O_RDONLY);
            if(fd < 0){
                printw("Server non avviato");
                return 0;
            }
            char buffer[512];
            int pid;
            if(read(fd, buffer, 512) < 0 || sscanf(buffer, "%d", &pid) != 1){
                printe("Impossibile leggere il pidfile (%s)", PID_FILE);
                return 1;
            }
            if(kill(pid, SIGTERM) < 0){
                printe("Impossibile terminare il processo");
                return 1;
            }
            close(fd);
            if(unlink(PID_FILE) < 0){
                printw("Impossibile eliminare il pidfile");
            }
            printi("Server arrestato");
            return 0;
        }
    }
    
    fd = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL);
    if(fd < 0){
        if(autostart){
            printi("Autostart ignorato (server già avviato)");
        } else {
            printe("Impossibile creare il pidfile. Server già avviato?");
        }
        return 1;
    } else {
        char buffer[512];
        int res = sprintf(buffer, "%d\n", getpid());
        res = write(fd, buffer, res);
        close(fd);
        if(res < 0){
            printe("Impossibile scrivere il pidfile");
            unlink(PID_FILE);
        }
    }
    
    // Handling di SIGTERM
    signal(SIGTERM, server_sigterm_handler);
    
    // Avvia server bluetooth
    bdaddr_t bdaddr;
    bacpy(&bdaddr, BDADDR_ANY);
    
    int ctrlserv = l2cap_create_socket(&bdaddr, L2CAP_PSM_HIDP_CTRL, L2CAP_LM_MASTER, 10);
    if(ctrlserv < 0){
        printe("Impossibile inizializzare il socket di controllo");
        return 1;
    }

    int intserv = l2cap_create_socket(&bdaddr, L2CAP_PSM_HIDP_INTR, L2CAP_LM_MASTER, 10);
    if(intserv < 0){
        close(ctrlserv);
        printe("Impossibile inizializzare il socket di interrupt");
        return 1;
    }
    
    printi("Server avviato");
    
    while(1){
        struct sockaddr_l2 addr;
        struct hidp_connadd_req req;
        socklen_t addrlen;
        bdaddr_t addr_src, addr_dst;
        int ctrlsock, intsock;
        memset(&addr, 0, sizeof(addr));
        memset(&req, 0, sizeof(req));
        addrlen = sizeof(addr);
        if ((ctrlsock = accept(ctrlserv, (struct sockaddr*)&addr, &addrlen)) < 0){
            printe("accept() control socket fallito: %d", errno);
            continue;
        }
        bacpy(&addr_dst, &addr.l2_bdaddr);
        if (getsockname(ctrlsock, (struct sockaddr*)&addr, &addrlen) < 0){
            printe("getsockname() control socket fallito: %d", errno);
            close(ctrlsock);
            continue;
        }
        bacpy(&addr_src, &addr.l2_bdaddr);
        if ((intsock = accept(intserv, (struct sockaddr*)&addr, &addrlen)) < 0){
            printe("accept() interrupt socket fallito: %d", errno);
            close(ctrlsock);
            continue;
        }
        if (bacmp(&addr_dst, &addr.l2_bdaddr)){
            printe("Gli indirizzi control e interrupt non corrispondono");
            close(ctrlsock);
            close(intsock);
            continue;
        }
        get_sdp_device_info(&addr_src, &addr_dst, &req);
        if (req.vendor == 0x054c && req.product == 0x0268){
            int pid = fork();
            if(pid < 0){
                printe("Impossibile eseguire fork()");
            } else if(pid == 0){
                // Chiudi socket server
                close(ctrlserv);
                close(intserv);
                // Esegui il client mainloop
                int res = client_loop(ctrlsock, intsock);
                // Chiudi i socket con il joystick
                close(ctrlsock);
                close(intsock);
                _Exit(res);
            } else {
                printi("Processo client avviato");
            }
        }
        // Dal parent
        close(ctrlsock);
        close(intsock);
    }
    
    // esterno
    return 0;
}

char ds3_control_packet_template[50]={
    0x52, // SET_REPORT, Output
    0X01, 0x00,
    0xff, 0x00, // Small Act (Timeout, activation)
    0xff, 0x00, // Big Act (Timeout, force)
    0x00, 0x00, 0x00, 0x00,
    0x00, // Led Mask (0x02 = Led1, 4=L2, 8=L3, 0x10=L4)
    // Led blink data
    // 0xff, sync/delay?, sync/delay?, Toff, Ton
    // Charging period: 0x40
    // Low battery period: 0x10 
    0xFF, 0x27, 0x10, 0x00, 0x40, // Led 4 (offset +15, +16)
    0xFF, 0x27, 0x10, 0x00, 0x40, // 3 (+20, +21)
    0xFF, 0x27, 0x10, 0x00, 0x40, // 2 (+25, +26)
    0xFF, 0x27, 0x10, 0x00, 0x40, // 1 (+30, +31)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

short get_threshold(char val){
    val -= 128;
    if(val > -10 && val < 10) val = 0;
    return val << 8;
}

int client_loop(int ctrl, int intr){
    // Imposta il signal handler di SIGTERM per questo processo
    signal(SIGTERM, client_sigterm_handler);
    
    // Ricarica la configurazione (per questo client)
    jngdsett_load(NULL);
    
    // Variabili usate ovunque
    
    unsigned char buffer[50];
    
    // Invia attivazione
    unsigned char enable_msg[] = {
        0x53, // SET_REPORT, Feature
        0xf4, 0x42, 0x03, 0x00, 0x00
    };
    send(ctrl, enable_msg, 6, 0);
    recv(ctrl, buffer, 50, 0);
    
    // Inizializza jng
    jng_state_t state;
    ds3_report_t report;
    
    memset(&state, 0, sizeof(jng_state_t));
    
    int jngfd = open("/dev/jng/driver", O_RDWR | O_NONBLOCK);
    if(jngfd < 0) return 1;
    
    // SO. So che le ioctl non falliscono, alias faccio quello che voglio
    ioctl(jngfd, JNGIOCSETINFO, &jng_info);
    
    ioctl(jngfd, JNGIOCSETMODE,   JNG_WMODE_NORMAL | JNG_RMODE_EVENT);
    ioctl(jngfd, JNGIOCSETEVMASK, JNG_EV_FB_FORCE | JNG_EV_FB_LED);
    
    int res;
    jngdsett_read("set_leds", &res);
    if(res){
        // Questo dimostra la flessibilità di joystick-ng
        unsigned int slot;
        ioctl(jngfd, JNGIOCGETSLOT, &slot);
        
        int dfd = open("/dev/jng/device", O_RDWR);
        ioctl(dfd, JNGIOCSETSLOT, slot);
        ioctl(dfd, JNGIOCSETMODE, JNG_MODE_EVENT);
        
        int l1 = 0, l2 = 0, l3 = 0, l4 = 0;
        char strres[256];
        
        if(res == 2){ // Led fissi
            jngdsett_read("fixed_leds", strres);
            l4 = strres[0] != '0';
            l3 = strres[1] != '0';
            l2 = strres[2] != '0';
            l1 = strres[3] != '0';
        } else { // In base allo slot
            slot += 1;
            if(slot == 1 || slot == 5 || slot == 8 || slot >= 10) l1 = 1;
            if(slot == 2 || slot == 6 || slot >= 9) l2 = 1;
            if(slot == 3 || slot >= 7) l3 = 1;
            if(slot >= 4) l4 = 1;
        }
        
        jng_event_t ev;
        ev.type = JNG_EV_FB_LED;
        
        ev.what  = JNG_FB_LED_1;
        ev.value = l1;
        write(dfd, &ev, sizeof(jng_event_t));
        
        ev.what  = JNG_FB_LED_2;
        ev.value = l2;
        write(dfd, &ev, sizeof(jng_event_t));
        
        ev.what  = JNG_FB_LED_3;
        ev.value = l3;
        write(dfd, &ev, sizeof(jng_event_t));
        
        ev.what  = JNG_FB_LED_4;
        ev.value = l4;
        write(dfd, &ev, sizeof(jng_event_t));
        
        close(dfd);
    }
    
    // Imposta il thread che si occupa di gestire l'output
    pthread_t sender_thread;
    sender_thread_arg_t arg = {
        .jngfd  = jngfd,
        .ctrlfd = ctrl,
        
        .blevel = 255
    };
    
    if(pthread_create(&sender_thread, NULL, sender_thread_loop, &arg) < 0){
        printw("Impossibile avviare il sender thread, no feedback disponibile");
    }
    
    int timeout;
    jngdsett_read("timeout", &timeout);
    
    time_t disconnect_time = time(NULL) + timeout;
    
    // Mainloop!
    while(1){
        if(recv(intr, buffer, 50, 0) != 50) break;
        // HID Data, Input (0xa1) Protocol code: keyboard (0x01) 
        if(!(buffer[0] == 0xa1 && buffer[1] == 0x01)) break;
        
        time_t now = time(NULL);
        
        // HID Modifiers: 0
        if(buffer[2] == 0x00){
            memcpy(&report, buffer + 1, sizeof(ds3_report_t));
            
            // Per il sender thread
            arg.blevel = report.blevel;
            
            // Passa da ds3_report_t a jng_state_t
            #define ds3_rep2keyp(k, jk) if((state.keyp.k = report.k) != 0) state.keys |= jk
            #define ds3_rep2key(c, jk, kp) if(c){state.keys |= jk;state.keyp.kp = 255;}
            state.keys          = 0;
            state.keyp.L3       = 0;
            state.keyp.R3       = 0;
            state.keyp.Start    = 0;
            state.keyp.Select   = 0;
            state.keyp.Options1 = 0;
            
            ds3_rep2keyp(A,     JNG_KEY_A);
            ds3_rep2keyp(B,     JNG_KEY_B);
            ds3_rep2keyp(X,     JNG_KEY_X);
            ds3_rep2keyp(Y,     JNG_KEY_Y);
            
            ds3_rep2keyp(L1,    JNG_KEY_L1);
            ds3_rep2keyp(R1,    JNG_KEY_R1);
            ds3_rep2keyp(L2,    JNG_KEY_L2);
            ds3_rep2keyp(R2,    JNG_KEY_R2);
            
            ds3_rep2keyp(Down,  JNG_KEY_DOWN);
            ds3_rep2keyp(Right, JNG_KEY_RIGHT);
            ds3_rep2keyp(Left,  JNG_KEY_LEFT);
            ds3_rep2keyp(Up,    JNG_KEY_UP);
            
            ds3_rep2key(report.keys & 0x02, JNG_KEY_L3,       L3);
            ds3_rep2key(report.keys & 0x04, JNG_KEY_R3,       R3);
            ds3_rep2key(report.keys & 0x08, JNG_KEY_START,    Start);
            ds3_rep2key(report.keys & 0x01, JNG_KEY_SELECT,   Select);
            ds3_rep2key(report.ps,          JNG_KEY_OPTIONS1, Options1);
            
            state.axis.LX = get_threshold(report.LX);
            state.axis.LY = get_threshold(report.LY);
            state.axis.RX = get_threshold(report.RX);
            state.axis.RY = get_threshold(report.RY);
            
            state.accelerometer.x = (report.accelX - 128) << 8;
            state.accelerometer.y = (report.accelY - 128) << 8;
            state.accelerometer.z = (report.accelZ - 128) << 8;
            state.gyrometer.x     = (report.gyroX - 128) << 8;
            
            write(jngfd, &state, sizeof(jng_state_t));
            
            // Controllo timeout
            if(timeout){
                if(state.keys != 0) disconnect_time = now + timeout;
                if(disconnect_time < now){
                    printi("Disconnessione per inattività");
                    close(ctrl);
                    close(intr);
                    return 0;
                }
            }
        }
    }
    printe("Connessione persa");
    return 0;
}

void* sender_thread_loop(void* varg){
    unsigned char ds3_control_packet[50];
    memcpy(ds3_control_packet, ds3_control_packet_template, 50);
    
    unsigned char dummy_buffer[50];
    
    sender_thread_arg_t* arg = (sender_thread_arg_t*)varg;
    jng_event_t ev;
    
    int blink;
    jngdsett_read("blink_leds", &blink);
    
    // Invia il primo pacchetto
    send(arg->ctrlfd, ds3_control_packet, 50, 0);
    recv(arg->ctrlfd, dummy_buffer, 50, 0);
    
    int lastblevel = 0;
    int changed    = 0;
    
    while(1){
        // Passa da jng_feedback_t al joystick
        if(read(arg->jngfd, &ev, sizeof(jng_event_t)) < 0){
            // Invia il report quando non ci sono più eventi in coda
            if(changed){
                if(send(arg->ctrlfd, ds3_control_packet, 50, 0) < 0){
                    printw("Impossibile inviare i dati al joystick");
                } else {
                    recv(arg->ctrlfd, dummy_buffer, 50, 0);
                }
                changed = 0;
            }
            
            usleep(FEEDBACK_POLL_TIME_USEC);
            continue;
        }
        
        changed = 1;
        switch(ev.type){
            case JNG_EV_FB_FORCE:
                switch(ev.what){
                    case JNG_FB_FORCE_SMALLMOTOR:
                        ds3_control_packet[4] = ev.value ? 255 : 0;
                        break;
                    case JNG_FB_FORCE_BIGMOTOR:
                        ds3_control_packet[6] = ev.value >> 8;
                        break;
                    default:
                        changed = 0;
                        break;
                }
                break;
            
            case JNG_EV_FB_LED: {
                char ledmask = 0;
                switch(ev.what){
                    case JNG_FB_LED_1:
                        ledmask = 0x02;
                        break;
                    case JNG_FB_LED_2:
                        ledmask = 0x04;
                        break;
                    case JNG_FB_LED_3:
                        ledmask = 0x08;
                        break;
                    case JNG_FB_LED_4:
                        ledmask = 0x10;
                        break;
                    default:
                        changed = 0;
                        break;
                }
                
                if(ev.value) ds3_control_packet[11] |=  ledmask;
                else         ds3_control_packet[11] &= ~ledmask;
                
                } break;
            
            default:
                changed = 0;
                break;
        }
        
        if(blink && arg->blevel != lastblevel){
            lastblevel = arg->blevel;
            
            // Normale
            unsigned char Ton  = 0x80;
            unsigned char Toff = 0x00;
            
            if(arg->blevel == 238){ // In carica
                Ton  = 0x40;
                Toff = 0x40;
            } else if(arg->blevel <= 1){ // Scarico
                Ton  = 0x10;
                Toff = 0x10;
            }
            
            ds3_control_packet[15] = Toff;
            ds3_control_packet[16] = Ton;
            
            ds3_control_packet[20] = Toff;
            ds3_control_packet[21] = Ton;
            
            ds3_control_packet[25] = Toff;
            ds3_control_packet[26] = Ton;
            
            ds3_control_packet[30] = Toff;
            ds3_control_packet[31] = Ton;
            
            changed |= 1;
        }
    }
    
    // Per il compilatore
    return NULL;
}

int l2cap_create_socket(const bdaddr_t *bdaddr, unsigned short psm, int lm, int backlog){
    struct sockaddr_l2 addr;
    struct l2cap_options opts;
    int sock;
    if ((sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    bacpy(&addr.l2_bdaddr, bdaddr);
    addr.l2_psm = htobs(psm);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        close(sock);
        return -1;
    }
    setsockopt(sock, SOL_L2CAP, L2CAP_LM, &lm, sizeof(lm));
    memset(&opts, 0, sizeof(opts));
    opts.imtu = 64;
    opts.omtu = HIDP_DEFAULT_MTU;
    opts.flush_to = 0xffff;
    setsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts));
    if (listen(sock, backlog) < 0){
        close(sock);
        return -1;
    }
    return sock;
}

// Copiato pari pari da libds3, prima o poi capirò cosa fa
int get_sdp_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req){
    struct sockaddr_l2 addr;
    socklen_t addrlen;
    bdaddr_t bdaddr;
    sdp_data_t *pdlist, *pdlist2;
    sdp_list_t *search, *attrid, *pnp_rsp, *hid_rsp;
    sdp_record_t *rec;
    sdp_session_t *sdp_session;
    uuid_t svclass;
    uint32_t range = 0x0000ffff;
    int err;
    sdp_session = sdp_connect(src, dst, SDP_RETRY_IF_BUSY | SDP_WAIT_ON_CLOSE);
    if (!sdp_session) return -1;
    sdp_uuid16_create(&svclass, PNP_INFO_SVCLASS_ID);
    search = sdp_list_append(NULL, &svclass);
    attrid = sdp_list_append(NULL, &range);
    err = sdp_service_search_attr_req(sdp_session, search, SDP_ATTR_REQ_RANGE, attrid, &pnp_rsp);
    sdp_list_free(search, NULL);
    sdp_list_free(attrid, NULL);
    sdp_uuid16_create(&svclass, HID_SVCLASS_ID);
    search = sdp_list_append(NULL, &svclass);
    attrid = sdp_list_append(NULL, &range);
    err = sdp_service_search_attr_req(sdp_session, search, SDP_ATTR_REQ_RANGE, attrid, &hid_rsp);
    sdp_list_free(search, NULL);
    sdp_list_free(attrid, NULL);
    memset(&addr, 0, sizeof(addr));
    addrlen = sizeof(addr);
    if (getsockname(sdp_session->sock, (struct sockaddr *) &addr, &addrlen) < 0) bacpy(&bdaddr, src);
    else bacpy(&bdaddr, &addr.l2_bdaddr);
    sdp_close(sdp_session);
    if (err || !hid_rsp) return -1;
    if (pnp_rsp) {
        rec = (sdp_record_t *) pnp_rsp->data;
        pdlist = sdp_data_get(rec, 0x0201);
        req->vendor = pdlist ? pdlist->val.uint16 : 0x0000;
        pdlist = sdp_data_get(rec, 0x0202);
        req->product = pdlist ? pdlist->val.uint16 : 0x0000;
        pdlist = sdp_data_get(rec, 0x0203);
        req->version = pdlist ? pdlist->val.uint16 : 0x0000;
        sdp_record_free(rec);
    }
    rec = (sdp_record_t *) hid_rsp->data;
    pdlist = sdp_data_get(rec, 0x0101);
    pdlist2 = sdp_data_get(rec, 0x0102);
    if (pdlist) {
        if (pdlist2) {
            if (strncmp(pdlist->val.str, pdlist2->val.str, 5)) {
                strncpy(req->name, pdlist2->val.str, sizeof(req->name) - 1);
                strcat(req->name, " ");
            }
            strncat(req->name, pdlist->val.str, sizeof(req->name) - strlen(req->name));
        } else strncpy(req->name, pdlist->val.str, sizeof(req->name) - 1);
    } else {
        pdlist2 = sdp_data_get(rec, 0x0100);
        if (pdlist2) strncpy(req->name, pdlist2->val.str, sizeof(req->name) - 1);
    }
    pdlist = sdp_data_get(rec, 0x0201);
    req->parser = pdlist ? pdlist->val.uint16 : 0x0100;
    pdlist = sdp_data_get(rec, 0x0202);
    req->subclass = pdlist ? pdlist->val.uint8 : 0;
    pdlist = sdp_data_get(rec, 0x0203);
    req->country = pdlist ? pdlist->val.uint8 : 0;
    pdlist = sdp_data_get(rec, 0x0206);
    if (pdlist){
        pdlist = pdlist->val.dataseq;
        pdlist = pdlist->val.dataseq;
        pdlist = pdlist->next;
        req->rd_data = (uint8_t*)malloc(pdlist->unitSize);
        if (req->rd_data) {
            memcpy(req->rd_data, (unsigned char*)pdlist->val.str, pdlist->unitSize);
            req->rd_size = pdlist->unitSize;
            // epox_endian_quirk - merge
            /* USAGE_PAGE (Keyboard)	05 07
             * USAGE_MINIMUM (0)		19 00
             * USAGE_MAXIMUM (65280)	2A 00 FF   <= must be FF 00
             * LOGICAL_MINIMUM (0)		15 00
             * LOGICAL_MAXIMUM (65280)	26 00 FF   <= must be FF 00
             */
            unsigned char pattern[] = {
                0x05, 0x07,
                0x19, 0x00,
                0x2a, 0x00, 0xff,
                0x15, 0x00,
                0x26, 0x00, 0xff
            };
            unsigned int i;
            if(req->rd_data){
                for (i = 0; i < req->rd_size - sizeof(pattern); i++) {
                    if (!memcmp(req->rd_data + i, pattern, sizeof(pattern))) {
                        req->rd_data[i +  5] = 0xff;
                        req->rd_data[i +  6] = 0x00;
                        req->rd_data[i + 10] = 0xff;
                        req->rd_data[i + 11] = 0x00;
                    }
                }
            }
        }
    }
    sdp_record_free(rec);
    return 0;
}

