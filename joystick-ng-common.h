#ifndef JOYSTICK_NG_COMMON
#define JOYSTICK_NG_COMMON 1

typedef struct{
    struct {
        unsigned int buttons;
        unsigned int buttons_mask;
        unsigned int hats;
        unsigned int axis;
        unsigned int axis_mask;
        unsigned int accelerometers;
        unsigned int gyrometers;
    } input;
    struct {
        unsigned int actuators;
        unsigned int leds;
    } output;
    unsigned int flags;
    char name[64];
    char driver[64];
} jng_device_info; // sizeof = 168

#include "joystick-ng-mask.h"

#define JNG_INFO_BUTTONS_PRESSURE 0x01 // Settato se il joystick ha uno o piu' sensori di pressione sui tasti
#define JNG_INFO_HATS_PRESSURE    0x02 // Settato se il joystick ha uno o piu' sensori di pressione sui punti di vista
#define JNG_INFO_LEDS_COLOR       0x10 // Settato se i led del joystick supportano una colorazione programmabile (DS4)

/// Input

#define JNG_X 0x20
#define JNG_Y 0x40
#define JNG_Z 0x80

typedef struct{
    unsigned int  Pressed;        // Tasti premuti (mod digitale)
    unsigned char A;              // |
    unsigned char B;              // |
    unsigned char C;              // | > ABXY, X, triangolo, cerchio, quadrato e altro
    unsigned char X;              // |
    unsigned char Y;              // |
    unsigned char Z;              // |
    unsigned char Start, Select;  // Su molti controller ci sono
    unsigned char L[6], R[6];     // Tasti dorsali e sulle levette
    unsigned char Options1;       // PS e roba varia
    unsigned char Options2;       // PS e roba varia
    unsigned char Other[8];       // Tasti specifici (Tipo il bottone nel touchpad del DS4)
} jng_buttons; // sizeof = 34

#define JNG_HAT_UNIDIRECTIONAL 0x01 // Non ha una posizione stabile intermedia
#define JNG_HAT_BIDIRECTIONAL  0x02 // Ha uno stato negativo, uno 0 e uno positivo (seguendo le coord. cartesiane)

typedef struct{
    struct jng_hats_state{
        short hor, ver;
        unsigned char htype, vtype;
    } State[4];
} jng_hats; // sizeof = 24

#define JNG_AXIS_ABSOLUTE 0x01
#define JNG_AXIS_RELATIVE 0x02

typedef struct{
    struct jng_axis_state{
        short val;
        unsigned char type;
    } L[4], R[4];
} jng_axis; // sizeof = 32

typedef struct{
    struct jng_accelerometers_state{
        short val;
        unsigned char type;
    } State[6];
} jng_accelerometers; // sizeof = 24

typedef struct{
    struct jng_gyrometers_state{
        short val;
        unsigned char type;
    } State[6];
} jng_gyrometers; // sizeof = 24

/// Output

typedef struct{
    struct jng_actuators_state{
        unsigned char speed;
        unsigned char timeout;
    } State[8];
} jng_actuators; // sizeof = 16

typedef struct{
    struct jng_leds_state{
        unsigned char R, G, B;
    } State[8];
} jng_leds; // sizeof = 24

typedef struct{
    jng_device_info    info;    // R  Info sul device        // sizeof = 168
    jng_buttons        buttons; // R  Bottoni premuti        // sizeof =  34
    jng_hats           hats;    // R  Punti di vista (DPad)  // sizeof =  24
    jng_axis           axis;    // R  Assi                   // sizeof =  32
    jng_accelerometers acc;     // R  Accelerometri          // sizeof =  24
    jng_gyrometers     gyro;    // R  Girometri              // sizeof =  24
    jng_actuators      act;     // W  Attuatori              // sizeof =  16
    jng_leds           leds;    // W  Led                    // sizeof =  24
    int                call;    // RW Chiamate configurate   // sizeof =   4
} jng_device_state; // sizeof = 352

#define JNG_INFO_CALL    0x01 // Legge le informazioni del joystick
#define JNG_BUTTONS_CALL 0x02 // Legge lo stato dei bottoni
#define JNG_HATS_CALL    0x04 // Legge lo stato dei punti di vista
#define JNG_AXIS_CALL    0x08 // Legge lo stato degli assi
#define JNG_ACC_CALL     0x10 // Legge lo stato degli accelerometri
#define JNG_GYRO_CALL    0x20 // Legge lo stato dei girometri
#define JNG_ACT_CALL     0x40 // Setta lo stato degli attuatori
#define JNG_LEDS_CALL    0x80 // Setta lo stato dei led

#endif
