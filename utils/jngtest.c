/*
 * jngtest.c
 * 
 * Copyright 2016-2019 Fabio Meneghetti <fabiomene97@gmail.com>
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

// jngtest: utilità per testare il modulo kernel e i driver per joystick-ng

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include "../include/joystick-ng.h"

typedef struct {
    unsigned int value;
    char* name;
} table_entry_t;

table_entry_t control_table[] = {
    {JNG_CTRL_CONNECTION,   "Connection"},
    {JNG_CTRL_INFO_CHANGED, "Info Changed"},
    {0}
};

table_entry_t key_table[] = {
    {JNG_KEY_A,        "A"},
    {JNG_KEY_B,        "B"},
    {JNG_KEY_C,        "C"},
    {JNG_KEY_X,        "X"},
    {JNG_KEY_Y,        "Y"},
    {JNG_KEY_Z,        "Z"},
    {JNG_KEY_L1,       "L1"},
    {JNG_KEY_R1,       "R1"},
    {JNG_KEY_L2,       "L2"},
    {JNG_KEY_R2,       "R2"},
    {JNG_KEY_L3,       "L3"},
    {JNG_KEY_R3,       "R3"},
    {JNG_KEY_DOWN,     "Down"},
    {JNG_KEY_RIGHT,    "Right"},
    {JNG_KEY_LEFT,     "Left"},
    {JNG_KEY_UP,       "Up"},
    {JNG_KEY_START,    "Start"},
    {JNG_KEY_SELECT,   "Select"},
    {JNG_KEY_OPTIONS1, "Options1"},
    {JNG_KEY_OPTIONS2, "Options2"},
    {JNG_KEY_OPTIONS2, "Options2"},
    {JNG_KEY_OTHER1,   "Other1"},
    {JNG_KEY_OTHER2,   "Other2"},
    {JNG_KEY_OTHER3,   "Other3"},
    {JNG_KEY_OTHER4,   "Other4"},
    {0}
};

table_entry_t axis_table[] = {
    {JNG_AXIS_LX, "Left X"},
    {JNG_AXIS_LY, "Left Y"},
    {JNG_AXIS_RX, "Right X"},
    {JNG_AXIS_RY, "Right Y"},
    {JNG_AXIS_G1, "Generic 1"},
    {JNG_AXIS_G2, "Generic 2"},
    {JNG_AXIS_G3, "Generic 3"},
    {JNG_AXIS_G4, "Generic 4"},
    {0}
};

table_entry_t sensor_table[] = {
    {JNG_SEN_ACCEL_X, "Accelerometer X"},
    {JNG_SEN_ACCEL_Y, "Accelerometer Y"},
    {JNG_SEN_ACCEL_Z, "Accelerometer Z"},
    {JNG_SEN_GYRO_X,  "Gyrometer X"},
    {JNG_SEN_GYRO_Y,  "Gyrometer Y"},
    {JNG_SEN_GYRO_Z,  "Gyrometer Z"},
    {0}
};

table_entry_t fb_force_table[] = {
    {JNG_FB_FORCE_BIGMOTOR,    "Big"},
    {JNG_FB_FORCE_SMALLMOTOR,  "Small"},
    {JNG_FB_FORCE_EXTRAMOTOR1, "Extra1"},
    {JNG_FB_FORCE_EXTRAMOTOR2, "Extra2"},
    {0}
};

unsigned int table_lookup_val(table_entry_t* table, char* name){
    int i = 0;
    while(table[i].value){
        if(strcasecmp(table[i].name, name) == 0) break;
        i++;
    }
    return table[i].value;
}

char lookup_buffer[1024];

char* table_lookup_name(table_entry_t* table, unsigned int value){
    int i = 0;
    while(table[i].value){
        if(table[i].value == value) return table[i].name;
        i++;
    }

    snprintf(lookup_buffer, 1023, "(unknown: %d)", value);
    
    return lookup_buffer;
}


void usage(char* fmt, ...){
    printf("Uso: jngtest <n> [opzioni]\n"
           "Interfaccia di test per joystick-ng\n"
           "Parametro <n>: slot da cui leggere (da 1 a 32)\n"
           "               Se si specifica g legge da tutti i joystick\n"
           "               (mod. aggregata) e sono validi solo i flag -kascA\n"
           "Se non vengono specificate opzioni viene sottointeso -i\n"
           "Le opzioni devono sempre essere specificate dopo <n>\n"
           "Opzioni:\n"
           "    -i          legge le informazioni del joystick\n"
           "    -k          legge lo stato dei tasti (KEY)\n"
           "    -a          legge lo stato degli assi (AXIS)\n"
           "    -s          legge lo stato dei sensori (SENSOR)\n"
           "    -c          legge lo stato del joystick (CTRL)\n"
           "    -A          leggi tutto l'input disponibile. Equivale a -kasc\n"
           "    -M m=<f>    imposta il valore del motore m a f\n"
           "                m è big, small, extra1 o extra2\n"
           "    -L l=<val>  imposta lo stato del led l. <val> può essere un numero puro o\n"
           "                esadecimale se viene anteposto 0x\n"
           "    -R          resetta lo stato di uscita del joystick (alias disattiva ogni feedback)\n"
           "    -h          mostra questo aiuto\n"
           "Nota: le opzioni -M e -L possono essere anche multiple\n");
    if(fmt){
        printf("Errore: ");
        va_list l;
        va_start(l, fmt);
        vprintf(fmt, l);
        va_end(l);
        printf("\n");
        exit(1);
    }
    exit(0);
}

int fd;

int aggregate = 0;

int read_info = 0;

unsigned int evmask = 0;

jng_event_ex_t event;

int main(int argc, char* argv[]){
    if(argc < 2) usage("Argomento <n> obbligatorio");
    
    int slot;
    
    if(strcmp(argv[1], "g") == 0){
        aggregate = 1;
    } else {
        slot = atoi(argv[1]) - 1;
        if(slot < 0 || slot > 32) usage("<n> deve essere compreso tra 1 e 32, oppure deve essere 'g'");
    }
    
    fd = open("/dev/jng/device", O_RDWR);
    if(fd < 0) {
        printf("Errore nell'apertura di /dev/jng/device\n");
        return 1;
    }
    if(!aggregate){
        ioctl(fd, JNGIOCSETSLOT, slot);
        ioctl(fd, JNGIOCSETMODE, JNG_MODE_EVENT);
    } else {
        ioctl(fd, JNGIOCSETMODE, JNG_MODE_EVENT | JNG_RMODE_AGGREGATE);
        
        int i;
        for(i = 0;i < 32;i++) ioctl(fd, JNGIOCAGRADD, i);
    }
    
    char option;
    if(argc > 2) while((option = getopt(argc - 1, argv + 1, "ikascAeM:L:Rh")) > 0) switch(option){
        case 'i':
            if(aggregate) usage("Opzione non permessa in mod. aggregata");
            read_info = 1;
            break;
        case 'k':
            evmask |= JNG_EV_KEY;
            break;
        case 'a':
            evmask |= JNG_EV_AXIS;
            break;
        case 's':
            evmask |= JNG_EV_SENSOR;
            break;
        case 'c':
            evmask |= JNG_EV_CTRL;
            break;
        case 'A':
            evmask |= JNG_EV_KEY | JNG_EV_AXIS | JNG_EV_SENSOR | JNG_EV_CTRL;
            break;
        case 'M': {
            if(aggregate) usage("Opzione non permessa in mod. aggregata");
            char* div = strchr(optarg, '=');
            if(!div) usage("L'opzione -M richiede il nome del motore, un uguale e il valore numerico");
            *div = 0;
            unsigned int m = table_lookup_val(fb_force_table, optarg);
            if(m == 0) usage("Il nome del motore non è valido");
            int val;
            if(sscanf(div+1, "%i", &val) != 1) usage("Valore del motore non inserito");
            if(val < 0) val = 0;
            if(val > 65535) val = 65535;
            event.type  = JNG_EV_FB_FORCE;
            event.what  = m;
            event.value = val;
            write(fd, &event, sizeof(jng_event_t));
        }; break;
        case 'L': {
            if(aggregate) usage("Opzione non permessa in mod. aggregata");
            char* div = strchr(optarg, '=');
            if(!div) usage("L'opzione -L richiede il numero del led, un uguale e il valore numerico");
            *div = 0;
            int l = atoi(optarg) - 1;
            if(l < 0 || l > 3) usage("Il numero del led deve essere compreso tra 1 e 4");
            int val;
            if(sscanf(div+1, "%i", &val) != 1) usage("Valore del led non inserito");
            event.type  = JNG_EV_FB_LED;
            event.what  = 1 << (l * 4);
            event.value = val;
            write(fd, &event, sizeof(jng_event_t));
        }; break;
        case 'R': {
            if(aggregate) usage("Opzione non permessa in mod. aggregata");
            jng_feedback_t fb;
            ioctl(fd, JNGIOCSETMODE, JNG_RMODE_EVENT | JNG_WMODE_BLOCK);
            memset(&fb, 0, sizeof(jng_feedback_t));
            write(fd, &fb, sizeof(jng_feedback_t));
            ioctl(fd, JNGIOCSETMODE, JNG_RMODE_EVENT | JNG_WMODE_EVENT);
        }; break;
        case 'h':
            usage(NULL);
        default:
            usage("Opzione '%c' non riconosciuta", option);
    }
    else read_info = !aggregate;
    
    if(read_info){
        jng_info_t info;
        ioctl(fd, JNGIOCGETINFO, &info);
        
        unsigned int i;
        
        printf("Joystick connesso: %s\n", info.connected ? "sì" : "no");
        
        printf("Nome: %s\n", info.name);
        
        printf("Alimentazione: %s\n", info.on_battery ? "batteria" : "cavo");
        
        printf("Input\n  Tasti presenti: ");
        for(i = 1;i <= JNG_KEY_MAX;i <<= 1){
            if(info.keys & i) printf("%s ", table_lookup_name(key_table, i));
        }
        
        printf("\n  Assi presenti: ");
        for(i = 1;i <= JNG_AXIS_MAX;i <<= 1){
            if(info.axis & i) printf("%s ", table_lookup_name(axis_table, i));
        }
        
        printf("\n  Sensori presenti: ");
        for(i = 1;i <= JNG_SEN_MAX;i <<= 1){
            if(info.sensors & i) printf("%s ", table_lookup_name(sensor_table, i));
        }
        
        printf("\nFeedback\n  Motori presenti: ");
        for(i = 1;i <= JNG_FB_FORCE_MAX;i <<= 1){
            if(info.fb_force & i) printf("%s ", table_lookup_name(fb_force_table, i));
        }
        
        printf("\n  Led presenti: ");
        for(i = 1;i <= JNG_FB_LED_MAX;i <<= 4){
            if(info.fb_led & i){
                printf("%d", JNG_C2I(i) >> 2);
                if(info.fb_led & (i << 1)) printf("+rgb");
                if(info.fb_led & (i << 2)) printf("+lum");
                printf(" ");
            }
        }
        
        printf("\nFlags\n");
        if(info.flags & JNG_FLAG_KEY_PRESSURE){
            printf("  KEY_PRESSURE: il joystick supporta la lettura della pressione su questi tasti\n    ");
            for(i = 1;i <= JNG_KEY_MAX;i <<= 1){
                if(info.keyp & i) printf("%s ", table_lookup_name(key_table, i));
            }
            printf("\n");
        }
    }
    
    if(!evmask) return 0;
    else ioctl(fd, JNGIOCSETEVMASK, evmask);
    
    while(1){
        read(fd, &event, sizeof(jng_event_ex_t));
        
        if(aggregate){
            printf("% 2d: ", event.num);
        }
        
        switch(event.type){
            case JNG_EV_CTRL:
                printf("CTRL %16s %d\n", table_lookup_name(control_table, event.what), event.value);
                break;
            case JNG_EV_KEY:
                printf("KEY  %16s %d\n", table_lookup_name(key_table, event.what), event.value);
                break;
            case JNG_EV_AXIS:
                printf("AXIS %16s %d\n", table_lookup_name(axis_table, event.what), event.value);
                break;
            case JNG_EV_SENSOR:
                printf("SEN  %16s %d\n", table_lookup_name(sensor_table, event.what), event.value);
                break;
        }

        fflush(stdout);
    }
    
    return 0;
}








