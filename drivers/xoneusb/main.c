/*
 * main.c
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

// Basato in gran parte su ../x360usb/main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <usb.h>
#include <syslog.h>
#include "../../include/joystick-ng.h"
#include "../../utils/libjngd-client/libjngd.h"

#define printd(fmt...) syslog(LOG_DEBUG,   fmt);
#define printi(fmt...) syslog(LOG_INFO,    fmt);
#define printw(fmt...) syslog(LOG_WARNING, fmt);
#define printe(fmt...) syslog(LOG_ERR,     fmt);

#define POLL_TIME_USEC 5000 // 5ms

jng_info_t jng_info = {
    .on_battery = 0,
    .keys       = JNG_KEY_ABXY | JNG_KEY_L1 | JNG_KEY_R1 | JNG_KEY_L2 | JNG_KEY_R2 | JNG_KEY_L3 | JNG_KEY_R3 | JNG_KEY_DIRECTIONAL | JNG_KEY_START | JNG_KEY_SELECT | JNG_KEY_OPTIONS1,
    .axis       = JNG_AXIS_LX | JNG_AXIS_LY | JNG_AXIS_RX | JNG_AXIS_RY,
    .sensors    = 0,
    .fb_force   = JNG_FB_FORCE_BIGMOTOR | JNG_FB_FORCE_SMALLMOTOR,
    .fb_led     = 0,
    .flags      = JNG_FLAG_KEY_PRESSURE,
    .keyp       = JNG_KEY_L2 | JNG_KEY_R2
};

jng_state_t state;

usb_dev_handle* handle;
int interface;

int jngfd;

typedef struct {
    unsigned char  keys1;   // lsb to msb: Y, X, B, A, Back, Start, N/A, N/A
    unsigned char  keys2;   //   "    "  : RS, LS, RB, LB, DLeft, DRight, DDown, DUp
    
    unsigned short LT;      // Triggers
    unsigned short RT;
    
             short LX;      // Axes
             short LY;
             short RX;
             short RY;
} xone_input_report_t;

typedef struct {
    unsigned char pressed;
    unsigned char reserved;
} xone_guide_report_t;

struct {
    unsigned char  type;    // Message type
    unsigned char  repsize; // Report size
    unsigned int   id;      // mah
    
    union {
        xone_input_report_t input;
        xone_guide_report_t guide;
    };
} report;


char xone_ff_out_report[] = {
    0x09, 0x08, 0x00,
    0x03, 0x00, // Continous rumble
    0x0f,       // Rumble mask
    0x04, 0x04, // ???
    0x00, // Big motor
    0x00, // Small motor
    0x00, 0x00
};

#define xone_ff_big   xone_ff_out_report[8]
#define xone_ff_small xone_ff_out_report[9]

void xone_send_ff_report(){
    int ur = usb_interrupt_write(handle, USB_ENDPOINT_OUT | 0x01, xone_ff_out_report, 12, 100);
    if(ur < 0){
        printw("Invio dati usb: %d", ur);
    }
}

int is_soft_disconnected = 0;

int axis_deadzone;

short get_threshold(short val){
    if(val > -axis_deadzone && val < axis_deadzone) val = 0;
    return val;
}

#define die(msg...) do{printe(msg);return 1;}while(0)

int main(int argc, char* argv[]){
    if(argc != 5){
        printf("Uso: %s busid devid prodstr padstr\nQuesto programma non andrebbe chiamato direttamente ma tramite jngctl\n", argv[0]);
        return 1;
    }
    
    openlog("jngd/xone", LOG_CONS | LOG_PID, LOG_DAEMON);
    
    printi("Init: %s %s '%s' '%s'", argv[1], argv[2], argv[3], argv[4]);
    
    
    int busid, devid;
    // busid e devid ci vengono dati da udev
    sscanf(argv[1], "%d", &busid);
    sscanf(argv[2], "%d", &devid);
    
    usb_init();
    usb_find_busses();
    usb_find_devices();
    
    // Controlli iniziali
    struct usb_bus* bus = usb_get_busses();
    
    while(bus && bus->location != busid) bus = bus->next;
    if(!bus) die("bus non trovato\n");
    
    struct usb_device* dev = bus->devices;
    while(dev && dev->devnum != devid) dev = dev->next;
    if(!dev) die("device non trovato\n");
    
    handle = usb_open(dev);
    if(!handle) die("impossibile aprire il device\n");
    
    // "Stacca" il driver kernel e imposta l'interfaccia come gestita in userspace
    usb_detach_kernel_driver_np(handle, interface);
    if(usb_claim_interface(handle, interface) < 0) die("impossibile bloccare l'interfaccia\n");

    
    // Imposta configurazione (usb_set_configuration non funziona per qualche motivo)
    usb_control_msg(handle, 0x00, 0x09, 0x01, 0x00, NULL, 0, 250);
    
    // Inizializza il pad
    char usb_init_packet[] = {0x05, 0x20, 0x00, 0x01, 0x00};
    usb_interrupt_write(handle, USB_ENDPOINT_OUT | 0x01, usb_init_packet, 5, 250);
    
    int usb_ret;
    
    jngd_set_drvoption_driver(NULL);
    
    // Avvio mainloop
    
    jngfd = open("/dev/jng/driver", O_RDWR | O_NONBLOCK);
    if(jngfd < 0) return 1;
    
    // Imposta il nome del controller
    int name_len = 0;
    if(strlen(argv[4]) == 0){
        name_len = snprintf((char*)jng_info.name, 255, "%s XBox ONE Controller (%s)", argv[3], argv[4]);
    } else {
        name_len = snprintf((char*)jng_info.name, 255, "%s XBox ONE Controller", argv[3]);
    }
    jng_info.name[name_len] = 0;
    
    ioctl(jngfd, JNGIOCSETINFO, &jng_info);
    
    ioctl(jngfd, JNGIOCSETMODE, JNG_RMODE_EVENT | JNG_WMODE_BLOCK);
    
    
    jngd_drvoption_get("axis_deadzone", JNGD_DRVOPT_TYPE_INT, &axis_deadzone); // Globali
    
    // Mainloop!
    while(1){
        usb_ret = usb_interrupt_read(handle, USB_ENDPOINT_IN | 0x01, (char*)&report, 64, 250);
        if(usb_ret < 0 && usb_ret != -16 && usb_ret != -110) break; // Boh, a volte genera errori 16 (Risorsa occupata) e all'inizio 110 (wireshark lo segna come 2)
        
        int data_read = 0;
        
        if(report.type == 0x20){ // Input Report
            // Preserva lo stato del tasto xbox, viene riportato in un altro pacchetto
            state.keys &= JNG_KEY_OPTIONS1;
            
            #define xone_rep2key(c, jk, kp) if(c){ state.keys |= jk; state.keyp.kp = 255; }
            
            xone_rep2key(report.input.keys1 & 0x01, JNG_KEY_Y,      Y);
            xone_rep2key(report.input.keys1 & 0x02, JNG_KEY_X,      X);
            xone_rep2key(report.input.keys1 & 0x04, JNG_KEY_B,      B);
            xone_rep2key(report.input.keys1 & 0x08, JNG_KEY_A,      A);
            xone_rep2key(report.input.keys1 & 0x10, JNG_KEY_SELECT, Select);
            xone_rep2key(report.input.keys1 & 0x20, JNG_KEY_START,  Start);
            
            xone_rep2key(report.input.keys2 & 0x01, JNG_KEY_R3,     R3);
            xone_rep2key(report.input.keys2 & 0x02, JNG_KEY_L3,     L3);
            xone_rep2key(report.input.keys2 & 0x04, JNG_KEY_R1,     R1);
            xone_rep2key(report.input.keys2 & 0x08, JNG_KEY_L1,     L1);
            xone_rep2key(report.input.keys2 & 0x01, JNG_KEY_LEFT,   Left);
            xone_rep2key(report.input.keys2 & 0x02, JNG_KEY_RIGHT,  Right);
            xone_rep2key(report.input.keys2 & 0x04, JNG_KEY_DOWN,   Down);
            xone_rep2key(report.input.keys2 & 0x08, JNG_KEY_UP,     Up);
            
            #undef xone_rep2key
            
            if((state.keyp.L2 = report.input.LT) > 0) state.keys |= JNG_KEY_L2;
            if((state.keyp.R2 = report.input.RT) > 0) state.keys |= JNG_KEY_R2;
            
            state.axis.LX = get_threshold(report.input.LX);
            state.axis.LY = get_threshold(-report.input.LY - 1);
            
            state.axis.RX = get_threshold(report.input.RX);
            state.axis.RY = get_threshold(-report.input.RY - 1);
            
            data_read = 1;
        } else if(report.type == 0x07){ // Tasto XBox
            // Imposta il tasto nel report
            
            if(report.guide.pressed){
                state.keys |= JNG_KEY_OPTIONS1;
                state.keyp.Options1 = 255;
            } else {
                state.keys &= ~(JNG_KEY_OPTIONS1);
                state.keyp.Options1 = 0;
            }
            
            // Caso speciale se il joystick è soft-disconnesso
            if(is_soft_disconnected && report.guide.pressed){
                ioctl(jngfd, JNGIOCSETINFO, &jng_info);
                
                is_soft_disconnected = 0;
            }
            
            data_read = 1;
        }
        
        if(data_read){
            // Aggiorna i dati nel kernel
            write(jngfd, &state, sizeof(jng_state_t));
        }
        
        // Consuma gli eventi
        int send_ff_report = 0;
        
        jng_event_t evt;
        while(read(jngfd, &evt, sizeof(jng_event_t)) > 0) switch(evt.type){
            case JNG_EV_CTRL:
                if(evt.type == JNG_CTRL_SOFT_DISCONNECT){
                    is_soft_disconnected = 1;
                }
                break;
            
            case JNG_EV_FB_FORCE:
                if     (evt.what == JNG_FB_FORCE_BIGMOTOR)   xone_ff_big   = evt.value >> 8;
                else if(evt.what == JNG_FB_FORCE_SMALLMOTOR) xone_ff_small = evt.value >> 8;
                else break;
                
                send_ff_report = 1;
                
                break;
        }
        
        if(send_ff_report) xone_send_ff_report();
        
        // Aspetta un po, per non sovraccaricare la CPU
        usleep(POLL_TIME_USEC);
    }
    
    // Il joystick è stato disconnesso, disconnettiamo anche noi
    printw("Ricezione dati fallita: %d\n", usb_ret);
    close(jngfd);
    usb_close(handle);
    return 0;
}

