#ifndef JOYSTICK_NG_MODULE_API
#define JOYSTICK_NG_MODULE_API 1

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include "joystick-ng-common.h"

#define MAX_GENERIC_DRIVERS  8
#define MAX_SPECIFIC_DRIVERS 32

typedef struct{
    unsigned short Vendor, Product, Version;
    unsigned char DevClass, DevSubClass, DevProto;
} jng_device_id;

/*
 * Da notare: Chiunque puo' aprire un device ma solo il primo puo'
 * controllare l'output
*/

typedef struct{
    struct usb_device*    usb_dev;
    void*                 private;
} jng_usb;

typedef struct{
    struct module* owner;
    char* name; // Nome conciso per il plugin
    // Procedure USB
    int  (*probe)(jng_device_id); // Ritorna 1 se il device deve essere gestito dal driver
    int  (*connect)(jng_usb*);    // Ritorna 1 su errore. Non e' detto che venga chiamata ogni volta che un device viene collegato (Consulta int plug(jng_usb*))
    void (*disconnect)(jng_usb*); // Come connect, al contrario
    // Procedure USB aggiuntive
    int  (*plug)(jng_usb*);   // Viene chiamato ogni volta che un device compatibile viene trovato
    void (*unplug)(jng_usb*);
    // Apertura e chiusura device
    void (*on_open)(jng_usb*);
    void (*on_close)(jng_usb*);
    // Procedure e basta
    // Controllo
    int  (*update)(jng_usb*); // Chiamata appena prima di qualsiasi chiamata alla sezione input
    int  (*commit)(jng_usb*); // Chiamata appena dopo qualsiasi chiamata alla sezione output
    //   Input
    void (*get_info)(jng_usb*, jng_device_info*);
    void (*get_buttons)(jng_usb*, jng_buttons*);
    void (*get_hats)(jng_usb*, jng_hats*);
    void (*get_axis)(jng_usb*, jng_axis*);
    void (*get_acc)(jng_usb*, jng_accelerometers*);
    void (*get_gyro)(jng_usb*, jng_gyrometers*);
    //   Output
    void (*set_act)(jng_usb*, jng_actuators*);
    void (*set_leds)(jng_usb*, jng_leds*);
} jng_driver;

#define JNG_DRIVER_GENERIC          0x000000
#define JNG_DRIVER_SPECIFIC         0x010000
#define JNG_DRIVER_HIGHEST_PRIORITY 0x000000
#define JNG_DRIVER_LOWEST_PRIORITY  0x000002

#ifdef JNG_BUILD_CORE
#define ExtDecl
#else
#define ExtDecl extern
#endif

ExtDecl int jng_add_driver(jng_driver*, int);
ExtDecl int jng_remove_driver(jng_driver*);

#endif
