#ifndef JOYSTICK_NG_FILE_FRONTEND
#define JOYSTICK_NG_FILE_FRONTEND 1

#include "jng_types.h"

typedef struct{
    u32 buttons;
    u32 axis;
    u32 sensors;
    u32 hats;
    s8  name[128];
} jng_joystick_info;
#define JNG_JOYSTICK_INFO 0x00

typedef struct 


#endif