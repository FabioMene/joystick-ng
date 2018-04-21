/*
 * joystick-ng.h
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

// Questo header è sia per user space che per kernel space
// Nel kernel va incluso SOLO joystick-ng-core.h
// In user space va incluso SOLO joystick-ng.h

// Strutture dati e nomi eventi, con ioctl e roba

#ifndef JOYSTICK_NG_H
#define JOYSTICK_NG_H 1

// Include kernel/user
#ifndef JOYSTICK_NG_CORE_H

// User space
#include <sys/ioctl.h>
#include <sys/types.h>

// ffs
#include <strings.h>

#else

// Kernel
#include <linux/ioctl.h>

#endif

#include <linux/types.h>

// INPUT (per il client)

// Stato del joystick (si ottiene da read in modalità normale)
typedef struct {
    /// Indicatore di stato. Questo campo è gestito dal kernel
    unsigned short connected;
    /// Tasti
    unsigned int keys;
    // Se il joystick supporta la lettura della pressione i dati appaiono qui, altrimenti i valori sono 0 e 255
    struct {
        // Tasti principali
        unsigned char A;
        unsigned char B;
        unsigned char C;
        unsigned char X;
        unsigned char Y;
        unsigned char Z;
        // Tasti dorsali/levette/grilletti
        unsigned char L1;
        unsigned char R1;
        unsigned char L2;
        unsigned char R2;
        unsigned char L3;
        unsigned char R3;
        // Tasti direzionali
        unsigned char Down;
        unsigned char Right;
        unsigned char Left;
        unsigned char Up;
        // Beh
        unsigned char Start;
        unsigned char Select;
        // Tasti di controllo (PS/X/Share)
        unsigned char Options1;
        unsigned char Options2;
        // Altri tasti
        unsigned char Other1;
        unsigned char Other2;
        unsigned char Other3;
        unsigned char Other4;
    } keyp;
    /// Assi
    struct {
        // Levetta sinistra
        short LX;
        short LY;
        // Levetta destra
        short RX;
        short RY;
        // Altri 4 assi generici
        short G1;
        short G2;
        short G3;
        short G4;
    } axis;
    /// Sensori
    struct {
        short x;
        short y;
        short z;
    } accelerometer;
    struct {
        short x;
        short y;
        short z;
    } gyrometer;
} jng_state_t;

// Evento joystick. what contiene UN SOLO BIT (si ottiene da read in mod evento, ovviamente). Può essere usato anche in output
typedef struct {
    unsigned short type;
    unsigned int   what;
    int            value;
} jng_event_t;

// Eventi di controllo
#define JNG_EV_CTRL        0x0001

#define JNG_CTRL_CONNECTED 0x00000001

#define __JNG_CTRL_MASK    0x00000001

// I tasti del joystick. Il modello è quello PS/Xbox
#define JNG_EV_KEY       0x0002

#define JNG_KEY_A        0x00000001
#define JNG_KEY_B        0x00000002
#define JNG_KEY_C        0x00000004
#define JNG_KEY_X        0x00000008
#define JNG_KEY_Y        0x00000010
#define JNG_KEY_Z        0x00000020
#define JNG_KEY_ABXY     (JNG_KEY_A | JNG_KEY_B | JNG_KEY_X | JNG_KEY_Y) // Per convenienza nelle definizioni, non è un tasto vero e proprio

#define JNG_KEY_L1       0x00000040
#define JNG_KEY_R1       0x00000080
#define JNG_KEY_L2       0x00000100
#define JNG_KEY_R2       0x00000200
#define JNG_KEY_L3       0x00000400
#define JNG_KEY_R3       0x00000800

#define JNG_KEY_DOWN     0x00001000
#define JNG_KEY_RIGHT    0x00002000
#define JNG_KEY_LEFT     0x00004000
#define JNG_KEY_UP       0x00008000
#define JNG_KEY_DIRECTIONAL (JNG_KEY_DOWN | JNG_KEY_RIGHT | JNG_KEY_LEFT | JNG_KEY_UP) // Per convenienza nelle definizioni, non è un tasto vero e proprio

#define JNG_KEY_START    0x00010000
#define JNG_KEY_SELECT   0x00020000

#define JNG_KEY_OPTIONS1 0x00040000
#define JNG_KEY_OPTIONS2 0x00080000

#define JNG_KEY_OTHER1   0x00100000
#define JNG_KEY_OTHER2   0x00200000
#define JNG_KEY_OTHER3   0x00400000
#define JNG_KEY_OTHER4   0x00800000

#define JNG_KEY_MAX      JNG_KEY_OTHER4

// Gli unici bit che i valori JNG_KEY_* possono assumere
#define __JNG_KEY_MASK   0x00ffffff

// Assi. X: valore + = destra, - = sinistra. Y: + = giù, - = su
#define JNG_EV_AXIS      0x0004

#define JNG_AXIS_LX      0x00000001
#define JNG_AXIS_LY      0x00000002
#define JNG_AXIS_RX      0x00000004
#define JNG_AXIS_RY      0x00000008
#define JNG_AXIS_G1      0x00000010
#define JNG_AXIS_G2      0x00000020
#define JNG_AXIS_G3      0x00000040
#define JNG_AXIS_G4      0x00000080

#define JNG_AXIS_MAX     JNG_AXIS_G4

#define __JNG_AXIS_MASK  0x000000ff

// Sensori
#define JNG_EV_SENSOR    0x0008

#define JNG_SEN_ACCEL_X  0x00000001
#define JNG_SEN_ACCEL_Y  0x00000002
#define JNG_SEN_ACCEL_Z  0x00000004
#define JNG_SEN_GYRO_X   0x00000008
#define JNG_SEN_GYRO_Y   0x00000010
#define JNG_SEN_GYRO_Z   0x00000020

#define JNG_SEN_MAX      JNG_SEN_GYRO_Z

#define __JNG_SEN_MASK   0x0000003f

// OUTPUT (per il client)

typedef struct {
    // Motori / forcefeedback
    struct {
        unsigned short bigmotor;
        unsigned short smallmotor;
        unsigned short extramotor1;
        unsigned short extramotor2;
    } force;
    // Durata effetto, in millisecondi. 0xffff vale a dire continuativo
    struct {
        unsigned short bigmotor;
        unsigned short smallmotor;
        unsigned short extramotor1;
        unsigned short extramotor2;
    } force_duration;
    // Leds, nel formato 0xAARRGGBB, dove AA è la luminosità
    struct {
        unsigned int led1;
        unsigned int led2;
        unsigned int led3;
        unsigned int led4;
    } leds;
} jng_feedback_t;

// Force Feedback
#define JNG_EV_FB_FORCE          0x0010

#define JNG_FB_FORCE_BIGMOTOR    0x00000001 // Un motore grande
#define JNG_FB_FORCE_SMALLMOTOR  0x00000002 // E uno piccolo, poco da dire
#define JNG_FB_FORCE_EXTRAMOTOR1 0x00000004 // Motori extra, dubito che esistano ma boh
#define JNG_FB_FORCE_EXTRAMOTOR2 0x00000008

// Durata effetto force feedback. Un client in modalità eventi che invia eventi di questo
// tipo 
#define JNG_FB_FORCEDUR_BIGMOTOR    0x00000010
#define JNG_FB_FORCEDUR_SMALLMOTOR  0x00000020
#define JNG_FB_FORCEDUR_EXTRAMOTOR1 0x00000040
#define JNG_FB_FORCEDUR_EXTRAMOTOR2 0x00000080

#define JNG_FB_FORCE_MAX         JNG_FB_FORCE_EXTRAMOTOR2

#define __JNG_FB_FORCE_MASK      0x0000000f


#define JNG_EV_FB_LED            0x0020

#define JNG_FB_LED_1             0x00000001 // Il led c'è, e fin qui ok
#define JNG_FB_LED_1_RGB         0x00000002 // Il led supporta i colori (non necessariamente rgb)
#define JNG_FB_LED_1_LUM         0x00000004 // Il led è regolabile in luminosità

#define JNG_FB_LED_2             0x00000010
#define JNG_FB_LED_2_RGB         0x00000020
#define JNG_FB_LED_2_LUM         0x00000040

#define JNG_FB_LED_3             0x00000100
#define JNG_FB_LED_3_RGB         0x00000200
#define JNG_FB_LED_3_LUM         0x00000400

#define JNG_FB_LED_4             0x00001000
#define JNG_FB_LED_4_RGB         0x00002000
#define JNG_FB_LED_4_LUM         0x00004000

#define JNG_FB_LED_MAX           JNG_FB_LED_4

// JNG_FB_LED_*_{RGB,LUM} vengono usati solo in info
#define __JNG_FB_LED_MASK        0x00001111

// IOCTL e strutture relative

// Per JNGIOCGETINFO
typedef struct {
    unsigned short connected; // Se è 0 i dati seguenti non sono attendibili. Questo campo è gestito dal kernel
    unsigned char  name[256]; // Il nome del joystick
    unsigned int   keys;      // I bit settati equivalgono ai tasti presenti sul joystick (JNG_KEY_*)
    unsigned int   axis;      // Come keys, per gli assi (JNG_AXIS_*)
    unsigned int   sensors;   // JNG_SEN_*
    
    unsigned int   fb_force;  // JNG_FB_FORCE_*
    unsigned int   fb_led;    // JNG_FB_LED_*
    
    unsigned int   flags;     // Vari flag relativi al joystick (JNG_FLAG_*)
    
    unsigned int   keyp;      // I bit settati equivalgono ai tasti (dejavu) che supportano la pressione variabile
} jng_info_t;

// I flag indicano le caratteristiche specifiche di ogni joystick

#define JNG_FLAG_KEY_PRESSURE 0x00000001 // Viene rilevata la pressione sui tasti (indicati in keyp)


// Per JNGIOCSETMODE
#define JNG_RMODE_NORMAL 0x00
#define JNG_RMODE_EVENT  0x01

#define JNG_WMODE_NORMAL 0x00
#define JNG_WMODE_EVENT  0x02

#define JNG_MODE_NORMAL  (JNG_RMODE_NORMAL | JNG_WMODE_NORMAL)
#define JNG_MODE_EVENT   (JNG_RMODE_EVENT  | JNG_WMODE_EVENT)


#define JNG_IOCTL_TYPE 'j'

// Imposta lo slot da cui si vuole leggere (C)
// Questo permette di selezionare il primo, secondo, ..., o trentaduesimo joystick (normalmente la prima chiamata dopo open)
// Se non eseguita read, write e le altre ioctl restituiranno errore
// Cambiando slot o cambiando modalità di lettura da normale ad eventi vengono rigenerati
// e messi in coda di lettura tutti gli eventi, come se lo stato precedente fosse non collegato.
// La coda può essere svuotata in qualsiasi momento chiamando fsync()
#define JNGIOCSETSLOT _IOW(JNG_IOCTL_TYPE, 0x00, unsigned int)


// Ottiene lo slot selezionato (CD)
#define JNGIOCGETSLOT _IOR(JNG_IOCTL_TYPE, 0x00, unsigned int)


// Imposta le informazioni del joystick (D)
#define JNGIOCSETINFO _IOW(JNG_IOCTL_TYPE, 0x01, jng_info_t)


// Ottiene le informazioni del joystick (CD)
#define JNGIOCGETINFO _IOR(JNG_IOCTL_TYPE, 0x01, jng_info_t)


// Imposta la modalità di lettura/scrittura (CD, esatto, anche i driver possono) tra normale ed eventi, tramite read() e write()
// La modalità consiste in UN flag JNG_RMODE_* e UN flag JNG_WMODE_*, ORati, oppure uno dei due flag JNG_MODE_*
// C e D indicano risp. client e driver e indicano cosa devono leggere/scrivere
// Mod Lettura Normale (JNG_RMODE_NORMAL e JNG_MODE_NORMAL)
//     C: jng_state_t
//     D: jng_feedback_t
// Mod Scrittura Normale (JNG_WMODE_NORMAL e JNG_MODE_NORMAL)
//     C: jng_feedback_t
//     D: jng_state_t
// Per le modalità ad eventi sia C che D leggono e scrivono jng_event_t
// Trovatemi un modulo più flessibile di questo
// Vedi anche: JNGIOCSETSLOT
#define JNGIOCSETMODE _IOW(JNG_IOCTL_TYPE, 0x02, unsigned int)

// Ottiene la modalità impostata (CD)
#define JNGIOCGETMODE _IOR(JNG_IOCTL_TYPE, 0x02, unsigned int)

// Imposta gli eventi a cui il client/driver (quindi CD) è interessato, valido ovviamente solo in mod. a eventi e solo per gli eventi ricevuti
// Di default tutti gli eventi vengono inviati
#define JNGIOCSETEVMASK _IOW(JNG_IOCTL_TYPE, 0x03, unsigned int)

// Ottiene gli eventi impostati (CD)
#define JNGIOCGETEVMASK _IOR(JNG_IOCTL_TYPE, 0x03, unsigned int)



// Hack per accedere velocemente ai dati del joystick

// Trasforma un codice (JNG_KEY_* / JNG_AXIS_* / JNG_SEN_* / JNG_FB_FORCE_*) in un indice. Serve più che altro per le prossime macro
#define JNG_C2I(c) ({int __i=ffs(c);__i?(__i-1):0;})


// Ottiene il dato pressione per il tasto k (p è jng_state_t)
#define JNG_KEYP(s, k) (*(((unsigned char*)&((s).keyp))+JNG_C2I(k)))

// Ottiene lo stato dell'asse a (s è jng_state_t)
#define JNG_AXIS(s, a) (*(((short*)&((s).axis))+JNG_C2I(a)))

// Ottiene lo stato del sensore sn (s è sempre jng_state_t)
#define JNG_SENSOR(s, sn) (*(((short*)&((s).accelerometer))+JNG_C2I(sn)))


// Ottiene la velocità del motore (f è jng_feedback_t)
#define JNG_FB_FORCE(f, m) (*(((unsigned short*)&((f).force))+JNG_C2I(m)))

// Ottiene la durata del force feedback (f è jng_feedback_t)
#define JNG_FB_FORCE(f, m) (*(((unsigned short*)&((f).force))+JNG_C2I(m)))

// Ottiene lo stato del led
#define JNG_FB_LED(f, l) (*(((unsigned int*)&((f).leds))+(JNG_C2I(l) >> 2)))


#endif

