#ifndef JOYSTICK_NG
#define JOYSTICK_NG 1

#include <linux/spinlock.h>
#define JNG_BUILD_CORE 1
#include "joystick-ng-module-api.h"

#define LOG_TAG "[joystick-ng]"
#define DEVICE_NAME "joystick-ng-dev"
#define CTRL_DEVICE_NAME "joystick-ng-ctrl"

#define printd(f, a...) printk(KERN_INFO  LOG_TAG "[Dbg ] " f "\n", ##a)
#define printw(f, a...) printk(KERN_ALERT LOG_TAG "[Warn] " f "\n", ##a)
#define printe(f, a...) printk(KERN_ERR   LOG_TAG "[Err ] " f "\n", ##a)


#define ACCESS(s) spin_lock(&s)
#define RELEASE(s) spin_unlock(&s)
#define RETURN(s, v) do{ RELEASE(s); return (v); } while(0)
#define ACCESS_D() ACCESS(jng_drivers_spinlock)
#define RELEASE_D() RELEASE(jng_drivers_spinlock)
#define RETURN_D(v) do{ RELEASE_D(); return (v); } while(0)
#define ACCESS_Dv() ACCESS(jng_devices_spinlock)
#define RELEASE_Dv() RELEASE(jng_devices_spinlock)
#define RETURN_Dv(v) do{ RELEASE_Dv(); return (v); } while(0)
#define ACCESS_J(i) ACCESS(jng_dev[i].spinlock)
#define RELEASE_J(i) RELEASE(jng_dev[i].spinlock)
#define RETURN_J(i, v) do{ RELEASE_J(i); return (v); } while(0)
#define J(i) jng_dev[i]

enum{
    DS_NOT_CONNECTED = 0,
    DS_NOT_PLUGGED,
    DS_CONNECTED
};

typedef struct{
    struct device* dev;       // puntatore al device /dev/jng*
    jng_driver*    driver;    // driver utilizzato
    jng_usb        usb_data;  // Dati USB e dati privati driver
    jng_device_id  usb_id;    // Id device usb
    unsigned int   connected; // 1 se il device e' connesso (0 solo se il device non esiste(slot vuoto) o qualche programma lo tiene aperto)
    unsigned int   opened;    // Numero di volte aperto
    unsigned int   state;     // Se lo stato e' DS_NOT_PLUGGED e un device con usb_id corrispondente viene collegato, non viene creato un nuovo device
    struct{
        pid_t pid;
        int call;
    }              calls[32]; // Numero di chiamata corrente (llseek) (uno per processo, fino a 32)
    int            rwpid;     // Pid (in user space) con capacita' di leggere e scrivere
    spinlock_t     spinlock;  // Spinlock di accesso
} jng_device;

#endif
