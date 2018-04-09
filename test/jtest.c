#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "joystick-ng/joystick-ng-common.h"

typedef struct {
    unsigned int val;
    char* name;
} table;

table btn_table[] = {
    { JNG_BTN_A,        "A" },
    { JNG_BTN_B,        "B" },
    { JNG_BTN_C,        "C" },
    { JNG_BTN_X,        "X" },
    { JNG_BTN_Y,        "Y" },
    { JNG_BTN_Z,        "Z" },
    { JNG_BTN_START,    "Start" },
    { JNG_BTN_SELECT,   "Select" },
    { JNG_BTN_L1,       "L1" },
    { JNG_BTN_L2,       "L2" },
    { JNG_BTN_L3,       "L3" },
    { JNG_BTN_L4,       "L4" },
    { JNG_BTN_L5,       "L5" },
    { JNG_BTN_L6,       "L6" },
    { JNG_BTN_R1,       "R1" },
    { JNG_BTN_R2,       "R2" },
    { JNG_BTN_R3,       "R3" },
    { JNG_BTN_R4,       "R4" },
    { JNG_BTN_R5,       "R5" },
    { JNG_BTN_R6,       "R6" },
    { JNG_BTN_OPTIONS1, "Opzione1" },
    { JNG_BTN_OPTIONS2, "Opzione2" },
    { JNG_BTN_OTH1,     "Altro1" },
    { JNG_BTN_OTH2,     "Altro2" },
    { JNG_BTN_OTH3,     "Altro3" },
    { JNG_BTN_OTH4,     "Altro4" },
    { JNG_BTN_OTH5,     "Altro5" },
    { JNG_BTN_OTH6,     "Altro6" },
    { JNG_BTN_OTH7,     "Altro7" },
    { JNG_BTN_OTH8,     "Altro8" },
    { 0,                NULL }
};

table axis_table[] = {
    { JNG_AX_L1, "L1" },
    { JNG_AX_L2, "L2" },
    { JNG_AX_L3, "L3" },
    { JNG_AX_L4, "L4" },
    { JNG_AX_R1, "R1" },
    { JNG_AX_R2, "R2" },
    { JNG_AX_R3, "R3" },
    { JNG_AX_R4, "R4" },
    { 0,         NULL }
};

char* lookup_table(table* _tab, unsigned int val){
    table* tab;
    for(tab=_tab;tab;tab++){
        if(tab->val == val) return tab->name;
    }
    return "(Sconosciuto)";
}

unsigned char lookup_btn_value(jng_buttons btns, unsigned int key){
    if(key == JNG_BTN_A)        return btns.A;
    if(key == JNG_BTN_B)        return btns.B;
    if(key == JNG_BTN_C)        return btns.C;
    if(key == JNG_BTN_X)        return btns.X;
    if(key == JNG_BTN_Y)        return btns.Y;
    if(key == JNG_BTN_Z)        return btns.Z;
    if(key == JNG_BTN_START)    return btns.Start;
    if(key == JNG_BTN_SELECT)   return btns.Select;
    if(key == JNG_BTN_L1)       return btns.L[0];
    if(key == JNG_BTN_L2)       return btns.L[1];
    if(key == JNG_BTN_L3)       return btns.L[2];
    if(key == JNG_BTN_L4)       return btns.L[3];
    if(key == JNG_BTN_L5)       return btns.L[4];
    if(key == JNG_BTN_L6)       return btns.L[5];
    if(key == JNG_BTN_R1)       return btns.R[0];
    if(key == JNG_BTN_R2)       return btns.R[1];
    if(key == JNG_BTN_R3)       return btns.R[2];
    if(key == JNG_BTN_R4)       return btns.R[3];
    if(key == JNG_BTN_R5)       return btns.R[4];
    if(key == JNG_BTN_R6)       return btns.R[5];
    if(key == JNG_BTN_OPTIONS1) return btns.Options1;
    if(key == JNG_BTN_OPTIONS2) return btns.Options2;
    if(key == JNG_BTN_OTH1)     return btns.Other[0];
    if(key == JNG_BTN_OTH2)     return btns.Other[1];
    if(key == JNG_BTN_OTH3)     return btns.Other[2];
    if(key == JNG_BTN_OTH4)     return btns.Other[3];
    if(key == JNG_BTN_OTH5)     return btns.Other[4];
    if(key == JNG_BTN_OTH6)     return btns.Other[5];
    if(key == JNG_BTN_OTH7)     return btns.Other[6];
    if(key == JNG_BTN_OTH8)     return btns.Other[7];
    return 0;
}

typedef struct {
    short val;
    unsigned char type;
} axis_value;

axis_value lookup_axis_value(jng_axis ax, unsigned int key){
    axis_value zero = {0, 0};
    axis_value* ptr = &zero;
    if(key == JNG_AX_L1) ptr = (axis_value*)&ax.L[0];
    if(key == JNG_AX_L2) ptr = (axis_value*)&ax.L[1];
    if(key == JNG_AX_L3) ptr = (axis_value*)&ax.L[2];
    if(key == JNG_AX_L4) ptr = (axis_value*)&ax.L[3];
    if(key == JNG_AX_R1) ptr = (axis_value*)&ax.R[0];
    if(key == JNG_AX_R2) ptr = (axis_value*)&ax.R[1];
    if(key == JNG_AX_R3) ptr = (axis_value*)&ax.R[2];
    if(key == JNG_AX_R4) ptr = (axis_value*)&ax.R[3];
    return *ptr;
}

void usage(char* pname, char* fmt, ...){
    if(fmt){
        va_list l;
        va_start(l, fmt);
        vprintf(fmt, l);
        va_end(l);
        printf("\n");
    }
    printf("Uso: %s [opzioni]\n"
           "Interfaccia di test per joystick-ng\n"
           "Opzioni:\n"
           "    -i          Leggi le informazioni del joystick\n"
           "    -b          Leggi lo stato dei tasti\n"
           "    -h          Leggi lo stato dei punti di vista\n"
           "    -a          Leggi lo stato degli assi\n"
           "    -m          Leggi lo stato degli accelerometri\n"
           "    -g          Leggi lo stato dei girometri\n"
           "    -A          Leggi tutto l'input disponibile. Equivale a -bhamg\n"
           "    -c          Modalita' continuativa (Solo per input)\n"
           "    -u ms       Tempo di aggiornamento in ms (Solo mod. continuativa)\n"
           "    -t m=f[,t]  Setta lo stato dell'attuatore m a f, con timeout t opzionale\n"
           "    -l l=RRGGBB Setta lo stato del led l a RRGGBB(Hex). Puo' essere anche RGB (Hex)\n"
           "    -d dev      Usa questo joystick. Puo' essere anche il solo numero\n"
           "    -H, -?      Mostra questo aiuto\n"
           "Nota: le opzioni -t e -l possono essere anche multiple\n"
           "Il device predefinito e' /dev/jng0\n"
           "Nel caso nessuna delle opzioni selezionate sia disponibile per il joystick\n"
           "il programma lo segnalera'\n", pname);
    if(fmt) exit(1);
    exit(0);
}

void read_state(int fd, jng_device_state* state){
    int ret;
    if((ret=read(fd, state, sizeof(jng_device_state))) < 0){
        printf("Errore durante la lettura del device: %s\n", strerror(-ret));
        exit(2);
    }
}

void write_state(int fd, jng_device_state* state){
    int ret;
    if((ret=write(fd, state, sizeof(jng_device_state))) < 0){
        printf("Errore durante la scrittura sul device: errno %d: %s\n", -ret, strerror(-ret));
        exit(2);
    }
}

jng_leds leds;
jng_actuators acts;

int running = 0;

void sighdl(int sig){
    running = 0;
    printf("\nInterruzione...\n");
}

#define printfs(f, a...) if(showinfo){printf(f, ##a);}

int main(int argc, char* argv[]){
    char device[128] = "/dev/jng0";
    int showinfo = 0;
    int mask = 0;
    int set_state = 0;
    int utime = 50;
    int request_in = 0, request_out = 0;
    int has_input = 0, has_output = 0;
    char option;
    int fd;
    unsigned int i;
    if(argc <= 1) usage(argv[0], NULL);
    while((option = getopt(argc, argv, "ibhamgAcu:t:l:d:H?")) != -1) switch(option){
        case 'i':
            showinfo = 1;
            break;
        case 'b':
            mask |= JNG_BUTTONS_CALL;
            break;
        case 'h':
            mask |= JNG_HATS_CALL;
            break;
        case 'a':
            mask |= JNG_AXIS_CALL;
            break;
        case 'm':
            mask |= JNG_ACC_CALL;
            break;
        case 'g':
            mask |= JNG_GYRO_CALL;
            break;
        case 'A':
            mask |= JNG_BUTTONS_CALL;
            mask |= JNG_HATS_CALL;
            mask |= JNG_AXIS_CALL;
            mask |= JNG_ACC_CALL;
            mask |= JNG_GYRO_CALL;
            break;
        case 'c':
            running = 1;
            signal(SIGINT, sighdl);
            break;
        case 'u':
            utime = atoi(optarg);
            break;
        case 't': {
                char* div = strchr(optarg, '=');
                if(!div) usage(argv[0], "L'opzione -t richiede il numero di attuatore, un uguale, la velocita' (o forza)\ne opzionalmente una virgola e il timeout");
                *div = 0;
                int m = atoi(optarg);
                int f = atoi(div+1);
                *div = '=';
                int t = 255;
                div = strchr(optarg, ',');
                if(div) t = atoi(div+1);
                if(m < 1 || m > 8) usage(argv[0], "Il motore deve essere compreso tra 1 e 8");
                m--;
                if(m < 0 || m > 255) usage(argv[0], "La velocita' deve essere compresa tra 0 e 255");
                if(t < 0 || t > 255) usage(argv[0], "Il timeout deve essere compreso tra 0 e 255");
                acts.State[m].speed   = f;
                acts.State[m].timeout = t;
                set_state |= JNG_ACT_CALL;
            }
            break;
        case 'l': {
                char* div = strchr(optarg, '=');
                if(!div) usage(argv[0], "L'opzione -l richiede il numero di led, un uguale e il valore colore in RGB o RRGGBB (Hex)");
                *div = 0;
                int l = atoi(optarg);
                if(l < 1 || l > 8) usage(argv[0], "Il led deve essere compreso tra 1 e 8");
                l--;
                int r, g, b;
                if(strlen(div+1) == 3){
                    if(sscanf(div+1, "%1x%1x%1x", &r, &g, &b) != 3) usage(argv[0], "Il valore esadecimale '%s' non e' corretto", div+1);
                    r*=16;
                    g*=16;
                    b*=16;
                } else if(strlen(div+1) == 6){
                    if(sscanf(div+1, "%2x%2x%2x", &r, &g, &b) != 3) usage(argv[0], "Il valore esadecimale '%s' non e' corretto", div+1);
                } else usage(argv[0], "E' necessario fornire un valore esadecimale nella forma RGB o RRGGBB");
                leds.State[l].R = r;
                leds.State[l].G = g;
                leds.State[l].B = b;
                set_state |= JNG_LEDS_CALL;
            }
            break;
        case 'd': {
                strcpy(device, optarg);
                int n;
                if(sscanf(device, "%d", &n) == 1){
                    sprintf(device, "/dev/jng%d", n);
                }
            }
            break;
        case 'H':
        case '?':
            usage(argv[0], NULL);
        default: usage(argv[0], NULL);
    }
    if(mask) request_in = 1;
    if(set_state) request_out = 1;
    if(request_in == 0 && request_out == 0 && showinfo == 0){
        printf("Nessun input o output selezionati\n");
        return 1;
    }
    utime*=1000;
    jng_device_state state;
    jng_device_info  info;
    fd = open(device, O_RDWR | O_LARGEFILE);
    if(fd < 0){
        printf("Errore durante l'apertura del device '%s'\n", device);
        return 1;
    }
    lseek(fd, JNG_INFO_CALL, 0);
    read_state(fd, &state);
    info = state.info;
    printfs("Joystick '%s', gestito dal driver '%s'\n", state.info.name, state.info.driver);
    printfs("Input disponibili:\n");
    if(info.input.buttons){
        has_input |= JNG_BUTTONS_CALL;
        printfs("    %d pulsant%c:", info.input.buttons, (info.input.buttons == 1)?'e':'i');
        for(i=1;i <= 0x20000000; i<<=1){
            if(info.input.buttons_mask & i) printfs(" %s", lookup_table(btn_table, i));
        }
        printfs("\n");
    }
    if(info.input.hats){
        has_input |= JNG_HATS_CALL;
        printfs("    %d punt%c di vista\n", info.input.hats, (info.input.hats == 1)?'o':'i');
    }
    if(info.input.axis){
        has_input |= JNG_AXIS_CALL;
        printfs("    %d ass%c:", info.input.axis, (info.input.axis == 1)?'e':'i');
        for(i=1;i <= 0x80; i<<=1){
            if(info.input.axis_mask & i) printfs(" %s", lookup_table(axis_table, i));
        }
        printfs("\n");
    }
    if(info.input.accelerometers){
        has_input |= JNG_ACC_CALL;
        printfs("    %d accelerometr%c\n", info.input.accelerometers, (info.input.accelerometers == 1)?'o':'i');
    }
    if(info.input.gyrometers){
        has_input |= JNG_GYRO_CALL;
        printfs("    %d girometr%c\n", info.input.gyrometers, (info.input.gyrometers == 1)?'o':'i');
    }
    if(has_input == 0) printfs("    (Nessuno)\n");
    printfs("Output disponibili:\n");
    if(info.output.actuators){
        has_output |= JNG_ACT_CALL;
        printfs("    %d attuator%c\n", info.output.actuators, (info.output.actuators == 1)?'e':'i');
    }
    if(info.output.leds){
        has_output |= JNG_LEDS_CALL;
        printfs("    %d led\n", info.output.leds);
    }
    if(has_output == 0) printfs("    (Nessuno)\n");
    if(has_input == 0 && has_output == 0){
        printf("Questo joystick non ha ne input ne output!\n");
        close(fd);
        return 0;
    }
    if(info.flags && showinfo){
        printf("Flags impostati:\n");
        if(info.flags & JNG_INFO_BUTTONS_PRESSURE) printf("    BUTTONS_PRESSURE: il joystick ha dei tasti non digitali (Grilletti, etc.)\n");
        if(info.flags & JNG_INFO_HATS_PRESSURE)    printf("    HATS_PRESSURE:    il joystick ha dei punti di vista non digitali\n");
        if(info.flags & JNG_INFO_LEDS_COLOR)       printf("    LEDS_COLOR:       il joystick ha dei led programmabili\n");
    }
    mask &= has_input;
    set_state &= has_output;
    if(mask == 0 && request_in){
        printf("Il joystick non ha nessuno degli input scelti\n");
        close(fd);
        return 3;
    }
    if(set_state == 0 && request_out){
        printf("Il joystick non ha nessuno degli output scelti\n");
        close(fd);
        return 3;
    }
    if(request_out){
        lseek(fd, set_state, 0);
        memcpy(&state.act,  &acts, sizeof(jng_actuators));
        memcpy(&state.leds, &leds, sizeof(jng_leds));
        write_state(fd, &state);
    }
    if(request_in){
        lseek(fd, mask, 0);
        do{
            read_state(fd, &state);
            printf("\x1b[H\x1b[J");
            if(mask & JNG_BUTTONS_CALL){
                printf("%08x ", state.buttons.Pressed);
                for(i=1;i <= 0x20000000; i<<=1){
                    if(info.input.buttons_mask & i) printf("%s: %3d  ", lookup_table(btn_table, i), lookup_btn_value(state.buttons, i));
                }
                printf("\n");
            }
            if(mask & JNG_HATS_CALL){
                for(i=0;i < info.input.hats;i++){
                    printf("%c%c/%d: %6d/%6d  ",
                        (state.hats.State[i].htype == JNG_HAT_UNIDIRECTIONAL)?'U':'B',
                        (state.hats.State[i].vtype == JNG_HAT_UNIDIRECTIONAL)?'U':'B',
                        i,
                        state.hats.State[i].hor,
                        state.hats.State[i].ver);
                }
                printf("\n");
            }
            if(mask & JNG_AXIS_CALL){
                for(i=1;i <= 0x80; i<<=1){
                    if(info.input.axis_mask & i){
                        axis_value ax = lookup_axis_value(state.axis, i);
                        printf("%c%c/%s: %6d  ",
                            (ax.type & JNG_AXIS_ABSOLUTE)?'A':'R',
                            (ax.type & JNG_X)?'X':((ax.type & JNG_Y)?'Y':'Z'),
                            lookup_table(axis_table, i),
                            ax.val);
                    }
                }
                printf("\n");
            }
            if(mask & JNG_ACC_CALL){
                for(i=0;i < info.input.accelerometers;i++){
                    printf("%c/%d: %6d  ",
                        (state.acc.State[i].type & JNG_X)?'X':((state.acc.State[i].type & JNG_Y)?'Y':'Z'),
                        i,
                        state.acc.State[i].val);
                }
                printf("\n");
            }
            if(mask & JNG_GYRO_CALL){
                for(i=0;i < info.input.gyrometers;i++){
                    printf("%c/%d: %6d  ",
                        (state.gyro.State[i].type & JNG_X)?'X':((state.gyro.State[i].type & JNG_Y)?'Y':'Z'),
                        i,
                        state.gyro.State[i].val);
                }
                printf("\n");
            }
            if(running) usleep(utime);
        } while(running);
    }
    close(fd);
    return 0;
}

