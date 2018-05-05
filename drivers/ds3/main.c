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


// Questo è un driver per joystick-ng per DualShock 3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <usb.h>
#include <syslog.h>

#include "../../include/joystick-ng.h"
#include "../../utils/libjngd-client/libjngd.h"

#include "ds3-packets.h"

#define printd(fmt...) syslog(LOG_DEBUG,   fmt);
#define printi(fmt...) syslog(LOG_INFO,    fmt);
#define printw(fmt...) syslog(LOG_WARNING, fmt);
#define printe(fmt...) syslog(LOG_ERR,     fmt);

#define POLL_TIME_USEC 5000 // 5ms

jng_info_t jng_info = {
    .name      = "Sony DualShock3",
    .keys      = JNG_KEY_ABXY | JNG_KEY_L1 | JNG_KEY_R1 | JNG_KEY_L2 | JNG_KEY_R2 | JNG_KEY_L3 | JNG_KEY_R3 | JNG_KEY_DIRECTIONAL | JNG_KEY_START | JNG_KEY_SELECT | JNG_KEY_OPTIONS1,
    .axis      = JNG_AXIS_LX | JNG_AXIS_LY | JNG_AXIS_RX | JNG_AXIS_RY,
    .sensors   = JNG_SEN_ACCEL_X | JNG_SEN_ACCEL_Y | JNG_SEN_ACCEL_Z | JNG_SEN_GYRO_X,
    .fb_force  = JNG_FB_FORCE_BIGMOTOR | JNG_FB_FORCE_SMALLMOTOR,
    .fb_led    = JNG_FB_LED_1 | JNG_FB_LED_2 | JNG_FB_LED_3 | JNG_FB_LED_4,
    .flags     = JNG_FLAG_KEY_PRESSURE,
    .keyp      = JNG_KEY_ABXY | JNG_KEY_L1 | JNG_KEY_R1 | JNG_KEY_L2 | JNG_KEY_R2 | JNG_KEY_DIRECTIONAL
};

jng_state_t state;

jng_feedback_t feedback;

usb_dev_handle* handle;
int interface;

int jngfd;

ds3_report_t report;

unsigned char ds3_control_packet[48];

int axis_deadzone = 10;

int blink_leds = 0;

void ds3_set_feedback(int cstate){
    // Motori
    ds3_control_packet[2] = ((unsigned int)feedback.force.smallmotor) ? 255 : 0;
    ds3_control_packet[4] = ((unsigned int)feedback.force.bigmotor) >> 8;
    
    // Led
    ds3_control_packet[9] = 0;
    if(feedback.leds.led1) ds3_control_packet[9] |= 0x02;
    if(feedback.leds.led2) ds3_control_packet[9] |= 0x04;
    if(feedback.leds.led3) ds3_control_packet[9] |= 0x08;
    if(feedback.leds.led4) ds3_control_packet[9] |= 0x10;
     
    // Lampeggiamento led
    unsigned char Toff = 0x00;
    if(blink_leds && cstate != 3) Toff = 0x40;
    ds3_control_packet[13] = Toff;
    ds3_control_packet[18] = Toff;
    ds3_control_packet[23] = Toff;
    ds3_control_packet[28] = Toff;
    
    // Invio dati
    int ur = usb_control_msg(handle, USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x09, 0x0201, 0, (char*)ds3_control_packet, 48, 100);
    if(ur < 0){
        printw("invio dati usb: %d\n", ur);
    }
}

short get_threshold(char val){
    val -= 128;
    if(val > -axis_deadzone && val < axis_deadzone) val = 0;
    return val << 8;
}

#define die(msg...) do{printe(msg);return 1;}while(0)

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("Uso: %s busid devid\nQuesto programma non andrebbe chiamato direttamente ma tramite jngctl\n", argv[0]);
        return 1;
    }
    
    openlog("jngd/ds3", LOG_CONS | LOG_PID, LOG_DAEMON);
    
    printi("Init: %s %s", argv[1], argv[2]);
    
    int busid, devid;
    // busid e devid ci vengono dati da udev
    sscanf(argv[1], "%d", &busid);
    sscanf(argv[2], "%d", &devid);
    
    // Inizializziamo libusb
    usb_init();
    usb_find_busses();
    usb_find_devices();
    
    // Trova il device
    struct usb_bus* bus = usb_get_busses();
    
    while(bus && bus->location != busid) bus = bus->next;
    if(!bus) die("bus non trovato\n"); // Non so come sia stato possibile, ma udev ha trovato un bus fantasma
    
    struct usb_device* dev = bus->devices;
    while(dev && dev->devnum != devid) dev = dev->next;
    if(!dev) die("device non trovato\n"); // Stessa storia, device fantasma
    
    // Controlliamo che dev sia effettivamente un ds3
    if(dev->descriptor.idVendor != 0x054c || dev->descriptor.idProduct != 0x0268) die("device non corrispondente\n");
    
    handle = usb_open(dev);
    if(!handle) die("impossibile aprire il device\n");
    
    // Otteniamo la configurazione che ci serve
    struct usb_config_descriptor* cfg;
    struct usb_interface_descriptor* alt;
    for(cfg = dev->config;cfg < dev->config + dev->descriptor.bNumConfigurations;cfg++){
        for(interface = 0;interface < cfg->bNumInterfaces;interface++){
            for(alt = cfg->interface[interface].altsetting;alt < cfg->interface[interface].altsetting + cfg->interface[interface].num_altsetting;alt++){
                if(alt->bInterfaceClass==3) goto _device_itf_ok;
            }
        }
    }
    // Configurazione non trovata, mah
    die("config non trovata\n");
    
  _device_itf_ok:
    // Finalmente abbiamo il device, togliamolo dalle fredde mani del kernel
    usb_detach_kernel_driver_np(handle, interface);
    if(usb_claim_interface(handle, interface) < 0) die("impossibile bloccare l'interfaccia\n");
    
    memcpy(ds3_control_packet, ds3_output_report_pkt, sizeof(ds3_output_report_pkt));
    
    int usb_ret;
    
    jngd_set_drvoption_driver(NULL);
    
    // Caricamento configurazione
    int res;
    jngd_drvoption_get("set_master_mac", JNGD_DRVOPT_TYPE_INT, &res);
    
    if(res){
        int mac[6], set_mac = 0;
        char strres[256];
        switch(res){
            case 2: // Prendi da hcitool dev
            case 3: {// Prova hcitool e fallback sulle impostazioni
                    FILE* hcitool = popen("hcitool dev", "r");
                    if(fscanf(hcitool, "%*s\n%x:%x:%x:%x:%x:%x", mac + 0, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5) == 6){
                        set_mac = 1;
                    }
                    pclose(hcitool);
                    if(res == 2) break;
                }
            case 1: // Prendi dalle impostazioni
            default:
                jngd_drvoption_get("master_mac", JNGD_DRVOPT_TYPE_STRING, strres);
                if(sscanf(strres, "%x:%x:%x:%x:%x:%x", mac + 0, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5) == 6){
                    set_mac = 1;
                }
                break;
        }
        if(set_mac){
            printd("Impostazione master mac a %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            char msg[8]= {
                0x01, 0x00,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
            };
            usb_ret = usb_control_msg(handle, USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x09, 0x03f5, 0, msg, sizeof(msg), 5000);
            if(usb_ret < 0) printw("set_master fallito: %d", usb_ret);
        } else {
            printw("Impossibile trovare un mac valido per l'impostazione set_master_mac");
        }
    }
    
    // Avvio mainloop
    
    // boh
    char ds3_setup_cmd[4] = {0x42, 0x0c, 0x00, 0x00};
    if((usb_ret = usb_control_msg(handle, USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x09, 0x03f4, 0, ds3_setup_cmd, 4, 100)) < 0) die("control setup fallito: %d\n", usb_ret);
    
    // ORA possiamo gestire jng
    
    jngfd = open("/dev/jng/driver", O_RDWR);
    if(jngfd < 0) return 1;
    
    // SO. So che le ioctl non falliscono, alias faccio quello che voglio
    ioctl(jngfd, JNGIOCSETINFO, &jng_info);
    
    unsigned int slot;
    ioctl(jngfd, JNGIOCGETSLOT, &slot);
    
    jngd_drvoption_get("set_leds", JNGD_DRVOPT_TYPE_INT, &res);
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
            jngd_drvoption_get("fixed_leds", JNGD_DRVOPT_TYPE_STRING, strres);
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
    
    jngd_drvoption_get("axis_deadzone", JNGD_DRVOPT_TYPE_INT, &axis_deadzone); // Globali
    axis_deadzone >>= 8;
    
    jngd_drvoption_get("blink_leds", JNGD_DRVOPT_TYPE_INT, &blink_leds);
    
    // Mainloop!
    while(1){
        if((usb_ret = usb_control_msg(handle, USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x01, 0x0101, 0, (char*)&report, sizeof(ds3_report_t), 100)) < 0) break;
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
        
        // Passa da jng_feedback_t al joystick
        read(jngfd, &feedback, sizeof(jng_feedback_t));
        
        ds3_set_feedback(report.cstate);
        
        // Aspetta un po, per non sovraccaricare la CPU
        usleep(POLL_TIME_USEC);
    }
    // Il joystick è stato disconnesso, disconnettiamo anche noi
    printw("ricezione dati fallita: %d\n", usb_ret);
    close(jngfd);
    usb_close(handle);
    return 0;
}

