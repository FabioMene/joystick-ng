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
    .name       = "Microsoft XBox 360 USB Controller",
    .on_battery = 0,
    .keys       = JNG_KEY_ABXY | JNG_KEY_L1 | JNG_KEY_R1 | JNG_KEY_L2 | JNG_KEY_R2 | JNG_KEY_L3 | JNG_KEY_R3 | JNG_KEY_DIRECTIONAL | JNG_KEY_START | JNG_KEY_SELECT | JNG_KEY_OPTIONS1,
    .axis       = JNG_AXIS_LX | JNG_AXIS_LY | JNG_AXIS_RX | JNG_AXIS_RY,
    .sensors    = 0,
    .fb_force   = JNG_FB_FORCE_BIGMOTOR | JNG_FB_FORCE_SMALLMOTOR,
    .fb_led     = JNG_FB_LED_1 | JNG_FB_LED_2 | JNG_FB_LED_3 | JNG_FB_LED_4,
    .flags      = JNG_FLAG_KEY_PRESSURE,
    .keyp       = JNG_KEY_L2 | JNG_KEY_R2
};

jng_state_t state;

usb_dev_handle* handle;
int interface;

int jngfd;

typedef struct {
    unsigned char  type;    // Message type (0x00: Button status, 0x01: LED status)
    unsigned char  repsize; // Report size (=20)
    unsigned char  keys1;   // lsb to msb: Dup, Ddown, Dleft, Dright, Start, Back, L3, R3
    unsigned char  keys2;   //   "    "  : LB, RB, XBox logo, Reserved, A, B, X, Y
    unsigned char  LT;      // Triggers
    unsigned char  RT;
    unsigned short LX;      // Axes
    unsigned short LY;
    unsigned short RX;
    unsigned short RY;
    unsigned char  res[3];
} x360_report_t;

x360_report_t report;

char x360_led_out_report[] = {
    0x01, 0x03,
    0x00 // Status: 0: all off, 1: blink, 2, 3, 4, 5 led flash then on (1 -> 4), 6, 7, 8, 9 led on (1 -> 4), 10 rotating
         // Modifiers (send after a Status report): 11: blinking, 12: slow blink, 13: animate then return to previous setting  
};

char x360_ff_out_report[] = {
    0x00, 0x08, 0x00,
    0x00, // Big motor
    0x00, // Small motor
    0x00, 0x00, 0x00
};


#define x360_led_status x360_led_out_report[2]

void x360_send_led_report(){
    int ur = usb_interrupt_write(handle, USB_ENDPOINT_OUT | 0x01, x360_led_out_report, 3, 100);
    if(ur < 0){
        printw("Invio dati usb: %d", ur);
    }
}


#define x360_ff_big   x360_ff_out_report[3]
#define x360_ff_small x360_ff_out_report[4]

void x360_send_ff_report(){
    int ur = usb_interrupt_write(handle, USB_ENDPOINT_OUT | 0x01, x360_ff_out_report, 8, 100);
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
    if(argc != 3){
        printf("Uso: %s busid devid\nQuesto programma non andrebbe chiamato direttamente ma tramite jngctl\n", argv[0]);
        return 1;
    }
    
    openlog("jngd/x360", LOG_CONS | LOG_PID, LOG_DAEMON);
    
    printi("Init: %s %s", argv[1], argv[2]);
    
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
    
    if(dev->descriptor.idVendor != 0x045e || dev->descriptor.idProduct != 0x028e) die("device non corrispondente\n");
    
    
    handle = usb_open(dev);
    if(!handle) die("impossibile aprire il device\n");
    
    // "Stacca" il driver kernel e imposta l'interfaccia come gestita in userspace
    usb_detach_kernel_driver_np(handle, interface);
    if(usb_claim_interface(handle, interface) < 0) die("impossibile bloccare l'interfaccia\n");

    
    unsigned char buffer[256];
    
    // Imposta configurazione (usb_set_configuration non funziona per qualche motivo)
    usb_control_msg(handle, 0x00, 0x09, 0x01, 0x00, NULL, 0, 250);
    
    int usb_ret;
    
    jngd_set_drvoption_driver(NULL);
    
    // Avvio mainloop
    
    jngfd = open("/dev/jng/driver", O_RDWR | O_NONBLOCK);
    if(jngfd < 0) return 1;
    
    ioctl(jngfd, JNGIOCSETINFO, &jng_info);
    
    ioctl(jngfd, JNGIOCSETMODE, JNG_RMODE_EVENT | JNG_WMODE_BLOCK);
    
    int set_led_on_slot_change = 0;
    
    int res;
    jngd_drvoption_get("set_leds", JNGD_DRVOPT_TYPE_INT, &res);
    
    if(res){
        unsigned int slot;
        ioctl(jngfd, JNGIOCGETSLOT, &slot);
        
        unsigned char led_status = 0;
        unsigned char led_status_mod = 0;
        
        char strres[256];
        
        if(res == 2){ // Led fissi
            jngd_drvoption_get("fixed_leds", JNGD_DRVOPT_TYPE_STRING, strres);
            
            if(strcmp(strres, "off") == 0){
                led_status = 0;
            } else if(strcmp(strres, "blink") == 0){
                led_status = 1;
            } else if(strcmp(strres, "slowblink") == 0){
                led_status = 1;
                led_status_mod = 12;
            } else if(strcmp(strres, "rotate") == 0){
                led_status = 10;
            } else if(sscanf(strres, "%d", &res) == 1 && res >= 1 && res <= 4){
                led_status = res + 5;
            } else {
                printi("Formato impostazione fixed_leds non valida, uso il valore di default");
                led_status = 1;
            }
        } else { // In base allo slot
            set_led_on_slot_change = 1;
            
            if(slot >= 0 && slot <= 3){
                led_status = slot + 2; // Blink then on
                led_status_mod = 0;
            } else if(slot >= 4 && slot <= 7){
                led_status = slot + 2;
                led_status_mod = 11;
            } else {
                led_status = 10;
            }
        }
        
        x360_led_status = led_status;
        x360_send_led_report();
        
        if(led_status_mod){
            x360_led_status = led_status_mod;
            x360_send_led_report();
        }
    }
    
    jngd_drvoption_get("axis_deadzone", JNGD_DRVOPT_TYPE_INT, &axis_deadzone); // Globali
    
    // Mainloop!
    while(1){
        usb_ret = usb_interrupt_read(handle, USB_ENDPOINT_IN | 0x01, (char*)buffer, 64, 250);
        if(usb_ret < 0 && usb_ret != -16 && usb_ret != -110) break; // Boh, a volte genera errori 16 (Risorsa occupata) e all'inizio 110 (wireshark lo segna come 2)
        
        memcpy(&report, buffer, sizeof(x360_report_t));
        
        if(report.type == 0){
            // Passa da x360_report_t a jng_state_t
            #define x360_rep2key(c, jk, kp) if(c){ state.keys |= jk; state.keyp.kp = 255; }
            memset(&state, 0, sizeof(jng_state_t));
            
            x360_rep2key(report.keys1 & 0x01, JNG_KEY_UP,     Up);
            x360_rep2key(report.keys1 & 0x02, JNG_KEY_DOWN,   Down);
            x360_rep2key(report.keys1 & 0x04, JNG_KEY_LEFT,   Left);
            x360_rep2key(report.keys1 & 0x08, JNG_KEY_RIGHT,  Right);
            x360_rep2key(report.keys1 & 0x10, JNG_KEY_START,  Start);
            x360_rep2key(report.keys1 & 0x20, JNG_KEY_SELECT, Select);
            x360_rep2key(report.keys1 & 0x40, JNG_KEY_L3,     L3);
            x360_rep2key(report.keys1 & 0x80, JNG_KEY_R3,     R3);
            
            x360_rep2key(report.keys2 & 0x01, JNG_KEY_L1,       L1);
            x360_rep2key(report.keys2 & 0x02, JNG_KEY_R1,       R1);
            x360_rep2key(report.keys2 & 0x04, JNG_KEY_OPTIONS1, Options1);
            x360_rep2key(report.keys2 & 0x10, JNG_KEY_A,        A);
            x360_rep2key(report.keys2 & 0x20, JNG_KEY_B,        B);
            x360_rep2key(report.keys2 & 0x40, JNG_KEY_X,        X);
            x360_rep2key(report.keys2 & 0x80, JNG_KEY_Y,        Y);
            
            #undef x360_rep2key
            
            if((state.keyp.L2 = report.LT) > 0) state.keys |= JNG_KEY_L2;
            if((state.keyp.R2 = report.RT) > 0) state.keys |= JNG_KEY_R2;
            
            state.axis.LX = get_threshold(report.LX);
            state.axis.LY = get_threshold(-report.LY - 1);
            
            state.axis.RX = get_threshold(report.RX);
            state.axis.RY = get_threshold(-report.RY - 1);
            
            write(jngfd, &state, sizeof(jng_state_t));
            
            if(is_soft_disconnected && (report.keys2 & 0x04)){
                ioctl(jngfd, JNGIOCSETINFO, &jng_info);
                
                is_soft_disconnected = 0;
            }
        }
        
        jng_event_t evt;
        if(read(jngfd, &evt, sizeof(jng_event_t)) > 0) switch(evt.type){
            case JNG_EV_CTRL:
                if(evt.type == JNG_CTRL_SLOT_CHANGED && set_led_on_slot_change){
                    if(evt.value >= 0 && evt.value <= 3){
                        x360_led_status = evt.value + 2; // Blink then on
                        x360_send_led_report();
                    } else if(evt.value >= 4 && evt.value <= 7){
                        x360_led_status = evt.value + 2;
                        x360_send_led_report();
                        
                        x360_led_status = 11;
                        x360_send_led_report();
                    } else {
                        x360_led_status = 10;
                        x360_send_led_report();
                    }
                } else if(evt.type == JNG_CTRL_SOFT_DISCONNECT){
                    is_soft_disconnected = 1;
                }
                break;
            
            case JNG_EV_FB_LED:
                if(evt.value == 0){
                    x360_led_status = 0;
                } else switch(evt.what){
                    case JNG_FB_LED_1:
                        x360_led_status = 6;
                        break;
                    case JNG_FB_LED_2:
                        x360_led_status = 7;
                        break;
                    case JNG_FB_LED_3:
                        x360_led_status = 8;
                        break;
                    case JNG_FB_LED_4:
                        x360_led_status = 9;
                        break;
                }
                x360_send_led_report();
                break;
            
            case JNG_EV_FB_FORCE:
                if     (evt.what == JNG_FB_FORCE_BIGMOTOR)   x360_ff_big   = evt.value >> 8;
                else if(evt.what == JNG_FB_FORCE_SMALLMOTOR) x360_ff_small = evt.value >> 8;
                else break;
                x360_send_ff_report();
                break;
        }
        
        // Aspetta un po, per non sovraccaricare la CPU
        usleep(POLL_TIME_USEC);
    }
    // Il joystick è stato disconnesso, disconnettiamo anche noi
    printw("Ricezione dati fallita: %d\n", usb_ret);
    close(jngfd);
    usb_close(handle);
    return 0;
}

