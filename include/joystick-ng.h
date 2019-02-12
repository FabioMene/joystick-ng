/*
 * joystick-ng.h
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

// tipi
#include <stdint.h>

// ffs
#include <strings.h>

#else

// Kernel
#include <linux/ioctl.h>

#endif

#include <linux/types.h>

// OUTPUT (per il client)

// Stato del joystick (si ottiene da read in modalità a blocchi)
typedef struct {
    /// Tasti
    uint32_t keys;
    // Se il joystick supporta la lettura della pressione i dati appaiono qui, altrimenti i valori sono 0 e 255
    struct {
        // Tasti principali
        uint8_t A;
        uint8_t B;
        uint8_t C;
        uint8_t X;
        uint8_t Y;
        uint8_t Z;
        // Tasti dorsali/levette/grilletti
        uint8_t L1;
        uint8_t R1;
        uint8_t L2;
        uint8_t R2;
        uint8_t L3;
        uint8_t R3;
        // Tasti direzionali
        uint8_t Down;
        uint8_t Right;
        uint8_t Left;
        uint8_t Up;
        // Beh
        uint8_t Start;
        uint8_t Select;
        // Tasti di controllo (PS/X/Share)
        uint8_t Options1;
        uint8_t Options2;
        // Altri tasti
        uint8_t Other1;
        uint8_t Other2;
        uint8_t Other3;
        uint8_t Other4;
    } keyp;
    /// Assi
    struct {
        // Levetta sinistra
        int16_t LX;
        int16_t LY;
        // Levetta destra
        int16_t RX;
        int16_t RY;
        // Altri 4 assi generici
        int16_t G1;
        int16_t G2;
        int16_t G3;
        int16_t G4;
    } axis;
    /// Sensori
    struct {
        int16_t x;
        int16_t y;
        int16_t z;
    } accelerometer;
    struct {
        int16_t x;
        int16_t y;
        int16_t z;
    } gyrometer;
} jng_state_t;


// Eventi di controllo client
typedef struct {
    uint8_t connected;
    uint32_t  last_info_inc; // L'incrementale dell'ultima volta che sono state impostate le info
} jng_client_control_t;

// Eventi di controllo driver
typedef struct {
    uint8_t soft_connected; // Se 0 il joystick è collegato ma non attivo (non appare nell'elenco)
    uint32_t  slot;
} jng_driver_control_t;

// In modalità a blocchi se si leggono sizeof(jng_state_t) + sizeof(jng_client_control_t) bytes
// read() legge entrambe le strutture (in sequenza). Queste strutture sono messe qui per convenienza
// Per i driver legge jng_feedback_t e jng_driver_control_t. La struttura jng_feedback_ex_t 
// è definita più in basso

typedef struct {
    jng_state_t          state;
    jng_client_control_t control;
} jng_state_ex_t;


// Evento. what contiene UN SOLO BIT (si ottiene da read in mod evento, ovviamente). 
// Può essere usato sia con read() che con write()

// Evento semplice.
// I driver possono usare solo questo tipo di evento
typedef struct {
    uint32_t type;   // JNG_EV_*
    uint32_t what;   // JNG_type_*
    int32_t  value;  // Il range del valore dipende dal tipo
} jng_event_t;

// Evento esteso, solo per client.
// In lettura num contiene il numero del joystick che ha generato l'evento,
// in scrittura indica il numero di joystick a cui inviare l'evento (indipendente da JNGIOCSETSLOT)
// I client possono usare anche eventi normali, in quel caso num viene scartato in lettura e assunto
// uguale al valore impostato con JNGIOCSETSLOT in scrittura
typedef struct {
    uint32_t type;
    uint32_t what;
    int32_t  value;
    uint32_t num;  // Numero del joystick
} jng_event_ex_t;



// Eventi di controllo. Questi eventi vengono inviati dal kernel
#define JNG_EV_CTRL        0x0001

/// Inviati ai client

// È cambiata la connessione del joystick.
// value contiene 0 se non connesso, nonzero se connesso
#define JNG_CTRL_CONNECTION   0x00000001 

// Sono cambiate le info del joystick. value non ha significato
#define JNG_CTRL_INFO_CHANGED 0x00000002

/// Inviati ai driver

// Il driver è stato disconnesso dal joystick (value non ha significato)
// Il driver decide l'azione da eseguire (per esempio mandare il joystick in idle se cablato)
// Il driver può riconnettere il joystick in qualsiasi momento usanto JNGIOCSETINFO
// Quando un driver è soft-disconnesso write() verrà ignorata e read() in modalità eventi
// ritornerà ENOTCONN una volta che la coda è vuota
#define JNG_CTRL_SOFT_DISCONNECT 0x10000001

// È cambiato lo slot assegnato al driver
#define JNG_CTRL_SLOT_CHANGED    0x10000002

#define __JNG_CLIENT_CTRL_MASK    0x00000003
#define __JNG_DRIVER_CTRL_MASK    0x10000003


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
        uint16_t bigmotor;
        uint16_t smallmotor;
        uint16_t extramotor1;
        uint16_t extramotor2;
    } force;
    // Durata effetto, in millisecondi. 0xffff vale a dire continuativo
    struct {
        uint16_t bigmotor;
        uint16_t smallmotor;
        uint16_t extramotor1;
        uint16_t extramotor2;
    } force_duration;
    // Leds, nel formato 0xAARRGGBB, dove AA è la luminosità
    struct {
        uint32_t led1;
        uint32_t led2;
        uint32_t led3;
        uint32_t led4;
    } leds;
} jng_feedback_t;

typedef struct {
    jng_feedback_t       feedback;
    jng_driver_control_t control;
} jng_feedback_ex_t;

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


/// IOCTL e strutture relative (driver e client)

// Per JNGIOCGETINFO
typedef struct {
    uint8_t connected; // Se è 0 i dati seguenti non sono attendibili. Questo campo è gestito dal kernel
    
    uint8_t on_battery; // Se è 0 il controller è alimentato, se nonzero a batteria
    
    uint8_t name[256]; // Il nome del joystick
    uint32_t  keys;      // I bit settati equivalgono ai tasti presenti sul joystick (JNG_KEY_*)
    uint32_t  axis;      // Come keys, per gli assi (JNG_AXIS_*)
    uint32_t  sensors;   // JNG_SEN_*
    
    uint32_t  fb_force;  // JNG_FB_FORCE_*
    uint32_t  fb_led;    // JNG_FB_LED_*
    
    uint32_t  flags;     // Vari flag relativi al joystick (JNG_FLAG_*)
    
    uint32_t  keyp;      // I bit settati equivalgono ai tasti (dejavu) che supportano la pressione variabile
} jng_info_t;


// I flag indicano le caratteristiche specifiche di ogni joystick

#define JNG_FLAG_KEY_PRESSURE 0x00000001 // Viene rilevata la pressione sui tasti (indicati in keyp)


// Per JNGIOCSETMODE
#define JNG_RMODE_BLOCK     0x00
#define JNG_RMODE_EVENT     0x01
#define JNG_RMODE_AGGREGATE 0x10 // Specificare sempre insieme a JNG_RMODE_EVENT

#define JNG_WMODE_BLOCK     0x00
#define JNG_WMODE_EVENT     0x02

#define JNG_MODE_BLOCK (JNG_RMODE_BLOCK | JNG_WMODE_BLOCK)
#define JNG_MODE_EVENT (JNG_RMODE_EVENT | JNG_WMODE_EVENT)

#define JNG_IOCTL_TYPE 'j'

// Imposta lo slot da cui si vuole leggere (C)
// Questo permette di selezionare il primo, secondo, ..., o trentaduesimo joystick (normalmente la prima chiamata dopo open)
// Se non eseguita read, write e le altre ioctl restituiranno errore
// Cambiando slot o cambiando modalità di lettura da normale ad eventi vengono rigenerati
// e messi in coda di lettura tutti gli eventi, come se lo stato precedente fosse non collegato.
// La coda può essere svuotata in qualsiasi momento chiamando fsync()
// Un client che legge in modalità aggregata (vedi sotto) e che scrive eventi estesi può ignorare questa ioctl
#define JNGIOCSETSLOT _IOW(JNG_IOCTL_TYPE, 0x00, uint32_t)


// Ottiene lo slot selezionato (CD)
#define JNGIOCGETSLOT _IOR(JNG_IOCTL_TYPE, 0x00, uint32_t)


// Imposta le informazioni del joystick (D)
#define JNGIOCSETINFO _IOW(JNG_IOCTL_TYPE, 0x01, jng_info_t)


// Ottiene le informazioni del joystick (CD)
#define JNGIOCGETINFO _IOR(JNG_IOCTL_TYPE, 0x01, jng_info_t)


// Imposta la modalità di lettura/scrittura (CD, esatto, anche i driver possono) tra normale ed eventi, tramite read() e write()
// La modalità consiste in UN flag JNG_RMODE_* e UN flag JNG_WMODE_*, ORati, oppure uno dei due flag JNG_MODE_*
// C e D indicano risp. client e driver e indicano cosa devono leggere/scrivere
// Mod Lettura Normale (JNG_RMODE_NORMAL e JNG_MODE_NORMAL)
//     C: jng_state_t o jng_state_ex_t
//     D: jng_feedback_t o jng_feedback_ex_t
// Mod Scrittura Normale (JNG_WMODE_NORMAL e JNG_MODE_NORMAL)
//     C: jng_feedback_t
//     D: jng_state_t
// Per le modalità ad eventi i client possono leggere e scrivere sia jng_event_t sia jng_event_ex_t
// mentre i driver leggono e scrivono solo jng_event_t
// Trovatemi un modulo più flessibile di questo
// Vedi anche: JNGIOCSETSLOT
#define JNGIOCSETMODE _IOW(JNG_IOCTL_TYPE, 0x02, uint32_t)

// Ottiene la modalità impostata (CD)
#define JNGIOCGETMODE _IOR(JNG_IOCTL_TYPE, 0x02, uint32_t)


// Imposta gli eventi a cui il client/driver (quindi CD) è interessato, valido ovviamente solo in mod. a eventi e solo per gli eventi ricevuti
// Di default tutti gli eventi meno JNG_EV_SENSORS vengono inviati
#define JNGIOCSETEVMASK _IOW(JNG_IOCTL_TYPE, 0x03, uint32_t)

// Ottiene gli eventi impostati (CD)
#define JNGIOCGETEVMASK _IOR(JNG_IOCTL_TYPE, 0x03, uint32_t)


// Modalità aggregata
// Questa modalità permette ai client (e solo ai client) di leggere da una lista di joystick
// invece che da uno solo. La modalità viene attivata impostando la modalità RMODE_EVENT | RMODE_AGGREGATE
// In modalità aggregata possono essere effettuate queste ioctl, che manipolano la lista di slot
// da cui leggere
// Le altre ioctl non sono influenzate da questa modalità, ad eccezione di
// JNGIOCSETEVMASK, che ha effetto su tutti i joystick della lista

// Aggiungi uno slot all'elenco (C)
// Aggiungere lo stesso slot rigenera i suoi eventi
#define JNGIOCAGRADD _IOW(JNG_IOCTL_TYPE, 0x04, uint32_t)

// Rimuove uno slot dall'elenco (C)
#define JNGIOCAGRDEL _IOW(JNG_IOCTL_TYPE, 0x05, uint32_t)


// Scarta tutti gli eventi in coda il cui tipo è presente nell'argomento di questa ioctl
#define JNGIOCDROPEVT _IOW(JNG_IOCTL_TYPE, 0x06, uint32_t)


/// IOCTL per control

#define JNG_CTRL_IOCTL_TYPE 'J'

// Disconnetti un joystick in software
#define JNGCTRLIOCSWDISC _IOW(JNG_CTRL_IOCTL_TYPE, 0x00, uint32_t)

// Scambia lo slot di 2 joystick
#define JNGCTRLIOCSWAPJS _IOW(JNG_CTRL_IOCTL_TYPE, 0x01, uint32_t[2])



/// Hack per accedere velocemente ai dati del joystick

// Trasforma un codice (JNG_KEY_* / JNG_AXIS_* / JNG_SEN_* / JNG_FB_FORCE_*) in un indice. Serve più che altro per le prossime macro
#define JNG_C2I(c) ({int32_t __i=ffs(c);__i?(__i-1):0;})


// Ottiene il dato pressione per il tasto k (p è jng_state_t)
#define JNG_KEYP(s, k) (*(((uint8_t*)&((s).keyp))+JNG_C2I(k)))

// Ottiene lo stato dell'asse a (s è jng_state_t)
#define JNG_AXIS(s, a) (*(((int16_t*)&((s).axis))+JNG_C2I(a)))

// Ottiene lo stato del sensore sn (s è sempre jng_state_t)
#define JNG_SENSOR(s, sn) (*(((int16_t*)&((s).accelerometer))+JNG_C2I(sn)))


// Ottiene la velocità del motore (f è jng_feedback_t)
#define JNG_FB_FORCE(f, m) (*(((uint16_t*)&((f).force))+JNG_C2I(m)))

// Ottiene la durata del force feedback (f è jng_feedback_t)
#define JNG_FB_FORCE(f, m) (*(((uint16_t*)&((f).force))+JNG_C2I(m)))

// Ottiene lo stato del led
#define JNG_FB_LED(f, l) (*(((uint32_t*)&((f).leds))+(JNG_C2I(l) >> 2)))


#endif

