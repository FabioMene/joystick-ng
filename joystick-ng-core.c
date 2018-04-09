#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/byteorder/generic.h>
#include <linux/types.h>
#include <linux/device.h>
#include "joystick-ng.h"
#include "joystick-ng-module-api.h"
#include "joystick-ng-common.h"

static struct class*  jng_class;

static struct cdev*   jng_ctrl_cdev;
static dev_t          jng_ctrl_dev;
static spinlock_t     jng_ctrl_spinlock;
static int            jng_ctrl_opened = 0;
static char           jng_ctrl_buffer[4096];
static int            jng_ctrl_buffer_off = 0;
static int            jng_ctrl_buffer_len = 0;

static struct cdev*   jng_cdev;
static jng_device     jng_dev[256];
static unsigned int   jng_connected_devs = 0;
static dev_t          jng_dev_base;
static spinlock_t     jng_drivers_spinlock;
static spinlock_t     jng_devices_spinlock;
static jng_driver*    jng_generic_drivers[MAX_GENERIC_DRIVERS];
static jng_driver*    jng_specific_drivers[MAX_SPECIFIC_DRIVERS];

static void jng_on_open_close_dummy(jng_usb* unused){
    // // // // // // // //
}

int jng_driver_verify_symbols(jng_driver* driver){
    if(!driver) return -2; // Non serve spiegare
    // Simboli essenziali
    if(!driver->owner)       return -3;
    if(!driver->probe)       return -2;
    if(!driver->connect)     return -2;
    if(!driver->disconnect)  return -2;
    if(!driver->update)      return -2;
    if(!driver->plug)        return -2;
    if(!driver->unplug)      return -2;
    if(!driver->commit)      return -2;
    if(!driver->get_info)    return -2;
    if(!driver->get_buttons) return -2;
    if(!driver->get_hats)    return -2;
    if(!driver->get_axis)    return -2;
    if(!driver->get_acc)     return -2;
    if(!driver->get_gyro)    return -2;
    if(!driver->set_act)     return -2;
    if(!driver->set_leds)    return -2;
    // Simboli opzionali
    if(!driver->on_open)  driver->on_open  = jng_on_open_close_dummy;
    if(!driver->on_close) driver->on_close = jng_on_open_close_dummy;
    return 0;
}

static int jng_add_specific_driver(jng_driver* driver, unsigned short flags){
    int i;
    if(flags & JNG_DRIVER_LOWEST_PRIORITY){
        for(i=MAX_SPECIFIC_DRIVERS-1;i >= 0;i--){
            if(jng_specific_drivers[i] == NULL) break;
        }
        if(i == -1) return -1;
    } else {
        for(i=0;i < MAX_SPECIFIC_DRIVERS;i++){
            if(jng_specific_drivers[i] == NULL) break;
        }
        if(i == MAX_SPECIFIC_DRIVERS) return -1;
    }
    jng_specific_drivers[i] = driver;
    printd("Nuovo driver specifico '%s' registrato", driver->name);
    return 0;
}

static int jng_add_generic_driver(jng_driver* driver, unsigned short flags){
    int i;
    if(flags & JNG_DRIVER_LOWEST_PRIORITY){
        for(i=MAX_GENERIC_DRIVERS-1;i >= 0;i--){
            if(jng_generic_drivers[i] == NULL) break;
        }
        if(i == -1) return -1;
    } else {
        for(i=0;i < MAX_GENERIC_DRIVERS;i++){
            if(jng_generic_drivers[i] == NULL) break;
        }
        if(i == MAX_GENERIC_DRIVERS) return -1;
    }
    jng_generic_drivers[i] = driver;
    printd("Nuovo driver generico '%s' registrato", driver->name);
    return 0;
}

int jng_add_driver(jng_driver* driver, int flags){
    int ret;
    ACCESS_D();
    ret = jng_driver_verify_symbols(driver);
    if(!ret){
        if(flags & JNG_DRIVER_SPECIFIC) ret = jng_add_specific_driver(driver, flags & 0xffff);
        else                            ret =  jng_add_generic_driver(driver, flags & 0xffff);
    }
    RELEASE_D();
    return ret;
}
EXPORT_SYMBOL(jng_add_driver);

static void jng_device_destroy(int i){ // ACCESS_J(i) e RELEASE_J(i) devono essere effettuati fuori
    ACCESS_D();
    J(i).dev = NULL;
    device_destroy(jng_class, MKDEV(MAJOR(jng_dev_base), i));
    RELEASE_D();
}

int jng_remove_driver(jng_driver* driver){
    int i;
    if(driver == NULL) return 1;
    ACCESS_D();
    ACCESS_Dv();
    for(i=0;i < 256;i++){
        if(J(i).driver == driver){
            J(i).state = DS_NOT_CONNECTED;
            jng_device_destroy(i);
        }
    }
    for(i=0;i < MAX_GENERIC_DRIVERS;i++){
        if(jng_generic_drivers[i] == driver){
            jng_generic_drivers[i] = NULL;
            printd("Driver generico '%s' rimosso", driver->name);
            RELEASE_Dv();
            RETURN_D(0);
        }
    }
    for(i=0;i < MAX_SPECIFIC_DRIVERS;i++){
        if(jng_specific_drivers[i] == driver){
            jng_specific_drivers[i] = NULL;
            printd("Driver specifico '%s' rimosso", driver->name);
            RELEASE_Dv();
            RETURN_D(0);
        }
    }
    RELEASE_Dv();
    RELEASE_D();
    return 1;
}
EXPORT_SYMBOL(jng_remove_driver);

static struct usb_device_id jng_usb_table[] = {
    { .driver_info = 42 },
    { },
};
MODULE_DEVICE_TABLE(usb, jng_usb_table);

static void jng_usb_descriptor_to_jng_id(struct usb_device_descriptor* desc, jng_device_id* id){
    id->Vendor      = le16_to_cpu(desc->idVendor);
    id->Product     = le16_to_cpu(desc->idProduct);
    id->Version     = le16_to_cpu(desc->bcdDevice);
    id->DevClass    = desc->bDeviceClass;
    id->DevSubClass = desc->bDeviceSubClass;
    id->DevProto    = desc->bDeviceProtocol;
}

static int jng_usb_probe(struct usb_interface* interface, const struct usb_device_id* unused){
    int i, j;
    jng_device_id id;
    jng_driver*   driver;
    ACCESS_D();
    ACCESS_Dv();
    jng_usb_descriptor_to_jng_id(&interface_to_usbdev(interface)->descriptor, &id);
    printd("Nuovo device %04x:%04x.%04x collegato", id.Vendor, id.Product, id.Version);
    for(i=0;i < 256;i++){
        if(J(i).state == DS_NOT_PLUGGED){
            if(memcmp(&J(i).usb_id, &id, sizeof(jng_device_id)) == 0){
                ACCESS_J(i);
                J(i).state = DS_CONNECTED;
                J(i).usb_data.usb_dev = interface_to_usbdev(interface);
                usb_set_intfdata(interface, (void*)i);
                J(i).driver->plug(&J(i).usb_data);
                J(i).connected = 1;
                try_module_get(J(i).driver->owner);
                RELEASE_J(i);
                RELEASE_Dv();
                RELEASE_D();
                return 0;
            }
        }
    }
    for(i=0;i < 256;i++){
        if(J(i).state == DS_NOT_PLUGGED){
            ACCESS_J(i);
            J(i).state = DS_NOT_CONNECTED;
            jng_connected_devs--;
            J(i).driver->disconnect(&J(i).usb_data);
            if(J(i).opened == 0) jng_device_destroy(i);
            J(i).connected = 0;
            memset(&J(i).usb_id, 0, sizeof(jng_device_id));
            RELEASE_J(i);
        }
    }
    if(jng_connected_devs >= 256){
        printd("Probing saltato perche' ci sono gia' 256 joystick collegati! WTF???");
        return -ENODEV;
    }
    for(i=0;i < MAX_SPECIFIC_DRIVERS;i++){
        if(jng_specific_drivers[i]){
            if(jng_specific_drivers[i]->probe(id)){
                driver = jng_specific_drivers[i];
                printd("Il driver '%s' ha una corrispondenza per questo device", driver->name);
                goto probe_find_devid;
            }
        }
    }
    for(i=0;i < MAX_GENERIC_DRIVERS;i++){
        if(jng_generic_drivers[i]){
            if(jng_generic_drivers[i]->probe(id)){
                driver = jng_generic_drivers[i];
                printd("Il driver '%s' ha una corrispondenza per questa categoria di device", driver->name);
                goto probe_find_devid;
            }
        }
    }
    printd("Nessun driver collegato");
    RELEASE_Dv();
    RETURN_D(-ENODEV);
  probe_find_devid:
    jng_connected_devs++;
    for(i=0;i < 256;i++){
        if(J(i).state == DS_NOT_CONNECTED) break;
    }
    memcpy(&J(i).usb_id, &id, sizeof(jng_device_id));
    J(i).dev                = device_create(jng_class, NULL, MKDEV(MAJOR(jng_dev_base), i), NULL, "jng%d", i);
    J(i).driver             = driver;
    J(i).usb_data.usb_dev   = interface_to_usbdev(interface);
    J(i).connected          = 1;
    J(i).opened             = 0;
    for(j=0;j < 32;j++){
        J(i).calls[j].pid  = -1;
        J(i).calls[j].call =  0;
    }
    J(i).rwpid              = -1;
    J(i).state = DS_CONNECTED;
    spin_lock_init(&J(i).spinlock);
    usb_set_intfdata(interface, (void*)i);
    try_module_get(driver->owner);
    driver->connect(&J(i).usb_data);
    driver->plug(&J(i).usb_data);
    RELEASE_Dv();
    RELEASE_D();
    return 0;
}

static void jng_usb_disconnect(struct usb_interface* interface){
    int i;
    i = (int)usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);
    ACCESS_J(i);
    J(i).usb_data.usb_dev = NULL;
    J(i).driver->unplug(&J(i).usb_data);
    J(i).state = DS_NOT_PLUGGED;
    module_put(J(i).driver->owner);
    RELEASE_J(i);
}

static struct usb_driver jng_usb_driver = {
    .name                 = "jng_generic_driver",
    .id_table             = jng_usb_table,
    .probe                = jng_usb_probe,
    .disconnect           = jng_usb_disconnect,
    .supports_autosuspend = 0,
    .soft_unbind          = 1
};

static int jng_dev_open(struct inode* inodp, struct file* fp){
    int i, j;
    i = iminor(inodp);
    fp->private_data = (void*)i;
    ACCESS_J(i);
    if(J(i).connected == 0) RETURN_J(i, -ENODEV); // Il device e' in vita solo perche' qualche altro programma lo sta usando, e' gia' disconnesso
    if(J(i).state == DS_NOT_PLUGGED) RETURN_J(i, -EAGAIN); // Il device e' in stallo, meglio evitare nuove aperture
    for(j=0;j < 32;j++){
        if(J(i).calls[j].pid == -1) break;
    }
    if(j == 32) RETURN_J(i, -ENODEV); // Se si arriva qui ci sono 32 processi che stanno gia' utilizzando questo joystick...
    J(i).calls[j].pid = current->pid;
    if(J(i).rwpid == -1) J(i).rwpid = current->pid;
    J(i).driver->on_open(&J(i).usb_data);
    J(i).opened++;
    try_module_get(THIS_MODULE);
    RELEASE_J(i);
    return 0;
}

static ssize_t jng_dev_read(struct file* fp, char* user_buffer, size_t length, loff_t* offset){
    int i, j, ret;
    jng_driver* driver;
    jng_device_state state;
    if(length != sizeof(jng_device_state)) return -EINVAL;
    memset(&state, 0, sizeof(jng_device_state));
    copy_to_user(user_buffer, &state, sizeof(jng_device_state));
    i = (int)fp->private_data;
    ACCESS_J(i);
    switch(J(i).state){
        case DS_NOT_CONNECTED: RETURN_J(i, -EIO);
        case DS_NOT_PLUGGED: RETURN_J(i, sizeof(jng_device_state));
    }
    driver = J(i).driver;
    for(j=0;j < 32;j++){
        if(J(i).calls[j].pid == current->pid) break;
    }
    #define UD &J(i).usb_data
    #define COND(n) (J(i).calls[j].call & (n))
    if((ret = driver->update(UD)) < 0){
        printd("Il driver ha riportato una condizione anomala: %d\n", ret);
        RETURN_J(i, -EIO);
    }
    if(COND(JNG_INFO_CALL)){
        driver->get_info(UD, &state.info);
        memset(state.info.driver, 0, 64);
        strcpy(state.info.driver, driver->name);
    }
    if(COND(JNG_BUTTONS_CALL)){
        driver->get_buttons(UD, &state.buttons);
    }
    if(COND(JNG_HATS_CALL))    driver->get_hats(UD, &state.hats);
    if(COND(JNG_AXIS_CALL))    driver->get_axis(UD, &state.axis);
    if(COND(JNG_ACC_CALL))     driver->get_acc(UD, &state.acc);
    if(COND(JNG_GYRO_CALL))    driver->get_gyro(UD, &state.gyro);
    state.call = J(i).calls[j].call;
    #undef COND
    #undef UD
    copy_to_user(user_buffer, &state, sizeof(jng_device_state));
    RELEASE_J(i);
    return length;
}

static ssize_t jng_dev_write(struct file* fp, const char* user_buffer, size_t length, loff_t* offset){
    int i, j, ret;
    jng_driver* driver;
    jng_device_state state;
    if(length != sizeof(jng_device_state)) return -EINVAL;
    i = (int)fp->private_data;
    ACCESS_J(i);
    switch(J(i).state){
        case DS_NOT_CONNECTED: RETURN_J(i, -EIO);
        case DS_NOT_PLUGGED: RETURN_J(i, sizeof(jng_device_state));
    }
    if(J(i).rwpid != current->pid) RETURN_J(i, -EINVAL);
    driver = J(i).driver;
    for(j=0;j < 32;j++){
        if(J(i).calls[j].pid == current->pid) break;
    }
    copy_from_user(&state, user_buffer, length);
    #define UD &J(i).usb_data
    #define COND(n) (J(i).calls[j].call & (n))
    if(COND(JNG_ACT_CALL))  driver->set_act(UD, &state.act);
    if(COND(JNG_LEDS_CALL)) driver->set_leds(UD, &state.leds);
    if((ret=driver->commit(UD)) < 0){
        printd("Il driver ha riportato una condizione anomala: %d\n", ret);
        RETURN_J(i, -EIO);
    }
    state.call = J(i).calls[j].call;
    #undef COND
    #undef UD
    RELEASE_J(i);
    return length;
}

static loff_t jng_dev_llseek(struct file* fp, loff_t call_code, int unused){
    int i, j;
    call_code &= 0xff;
    i = (int)fp->private_data;
    ACCESS_J(i);
    switch(J(i).state){
        case DS_NOT_CONNECTED: RETURN_J(i, -EIO);
        case DS_NOT_PLUGGED: RETURN_J(i, 0);
    }
    for(j=0;j < 32;j++){
        if(J(i).calls[j].pid == current->pid) break;
    }
    J(i).calls[j].call = call_code;
    RELEASE_J(i);
    return call_code;
}

static int jng_dev_close(struct inode* inodp, struct file* fp){
    int i, j;
    jng_driver* driver;
    i = iminor(inodp);
    ACCESS_J(i);
    driver = J(i).driver;
    J(i).driver->on_close(&J(i).usb_data);
    J(i).opened--;
    if(!J(i).connected && J(i).opened == 0) jng_device_destroy(i);
    for(j=0;j < 32;j++){
        if(J(i).calls[j].pid == current->pid){
            J(i).calls[j].pid = -1;
            break;
        }
    }
    if(J(i).rwpid == current->pid) J(i).rwpid = -1;
    module_put(THIS_MODULE);
    RELEASE_J(i);
    return 0;
}

static struct file_operations jng_file_operations = {
    .owner   = THIS_MODULE,
    .open    = jng_dev_open,
    .read    = jng_dev_read,
    .write   = jng_dev_write,
    .llseek  = jng_dev_llseek,
    .release = jng_dev_close
};

static int jng_ctrl_open(struct inode* inodp, struct file* fp){
    int i;
    char opt[128];
    ACCESS(jng_ctrl_spinlock);
    if(jng_ctrl_opened) RETURN(jng_ctrl_spinlock, -EAGAIN);
    jng_ctrl_opened = 1;
    RELEASE(jng_ctrl_spinlock);
    ACCESS_D();
    strcpy(jng_ctrl_buffer, "Driver specifici caricati:\n");
    for(i=0;i < MAX_SPECIFIC_DRIVERS;i++){
        if(jng_specific_drivers[i]){
            snprintf(opt, 128, "  %x - %s\n", i, jng_specific_drivers[i]->name);
            strcat(jng_ctrl_buffer, opt);
        }
    }
    strcpy(jng_ctrl_buffer, "Driver generici caricati:\n");
    for(i=0;i < MAX_GENERIC_DRIVERS;i++){
        if(jng_generic_drivers[i]){
            snprintf(opt, 128, "  %x - %s\n", i, jng_generic_drivers[i]->name);
            strcat(jng_ctrl_buffer, opt);
        }
    }
    jng_ctrl_buffer_off = 0;
    jng_ctrl_buffer_len = strlen(jng_ctrl_buffer);
    RELEASE_D();
    return 0;
}

static ssize_t jng_ctrl_read(struct file* fp, char* user_buffer, size_t length, loff_t* offset){
    if(jng_ctrl_buffer_off >= jng_ctrl_buffer_len) return 0;
    if(jng_ctrl_buffer_off+length >= jng_ctrl_buffer_len) length = jng_ctrl_buffer_len-jng_ctrl_buffer_off;
    copy_to_user(user_buffer, jng_ctrl_buffer+jng_ctrl_buffer_off, length);
    jng_ctrl_buffer_off+=length;
    return length;
}

static ssize_t jng_ctrl_write(struct file* fp, const char* user_buffer, size_t length, loff_t* offset){
    int off = 0, i, n;
    char cmds[40];
    char sn[3] = {0x00, 0x00, 0x00};
    char c;
    struct module* mod = NULL;
    for(i=0;i < length;i++){
        copy_from_user(&c, user_buffer+i, 1);
        if(c == '\n');
        else {
            cmds[off++] = c;
            if(off == 40) return -EINVAL;
        }
    }
    for(i=0;i < off;i+=4){
        sn[0] = cmds[i+1];
        sn[1] = cmds[i+2];
        sscanf(sn, "%x", &n);
        switch(cmds[i+0]){
            case 's':
                if(n < 0 || n > MAX_SPECIFIC_DRIVERS) return -EINVAL;
                if(jng_specific_drivers[n] == NULL) return -EINVAL;
                mod = jng_specific_drivers[n]->owner;
                break;
            case 'g':
                if(n < 0 || n > MAX_GENERIC_DRIVERS) return -EINVAL;
                if(jng_generic_drivers[n] == NULL) return -EINVAL;
                mod = jng_generic_drivers[n]->owner;
                break;
            default: return -EINVAL;
        }
        switch(cmds[i+3]){
            case '+':
                try_module_get(mod);
                break;
            case '-':
                module_put(mod);
                break;
            default: return -EINVAL;
        }
    }
    return 0;
}

static int jng_ctrl_close(struct inode* inodp, struct file* fp){
    ACCESS(jng_ctrl_spinlock);
    jng_ctrl_opened = 0;
    RELEASE(jng_ctrl_spinlock);
    return 0;
}

static struct file_operations jng_ctrl_operations = {
    .owner   = THIS_MODULE,
    .open    = jng_ctrl_open,
    .read    = jng_ctrl_read,
    .write   = jng_ctrl_write,
    .release = jng_ctrl_close
};

static int device_set_permissions(struct device* dev, struct kobj_uevent_env* env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init jng_init(void){
    spin_lock_init(&jng_drivers_spinlock);
    spin_lock_init(&jng_devices_spinlock);
    spin_lock_init(&jng_ctrl_spinlock);
    printd("Creazione classe device");
    jng_class = class_create(THIS_MODULE, "test-device-class");
    if(jng_class == NULL){
        printe("Impossibile creare una classe device");
        return 1;
    }
    jng_class->dev_uevent = device_set_permissions;
    printd("Inizializzazione controllo modulo");
    alloc_chrdev_region(&jng_ctrl_dev, 0, 1, CTRL_DEVICE_NAME);
    jng_ctrl_cdev = cdev_alloc();
    cdev_init(jng_ctrl_cdev, &jng_ctrl_operations);
    cdev_add(jng_cdev, jng_ctrl_dev, 1);
    device_create(jng_class, NULL, jng_ctrl_dev, NULL, "jng_ctrl");
    printd("Inizializzazione regione device");
    alloc_chrdev_region(&jng_dev_base, 0, 256, DEVICE_NAME);
    printd("Associazione device");
    jng_cdev = cdev_alloc();
    cdev_init(jng_cdev, &jng_file_operations);
    cdev_add(jng_cdev, jng_dev_base, 256);
    printd("Registrazione driver...");
    return usb_register(&jng_usb_driver);
}

static void __exit jng_exit(void){
    int i;
    ACCESS_D();
    ACCESS_Dv();
    printd("Deregistrazione driver");
    usb_deregister(&jng_usb_driver);
    printd("Distaccando i device");
    cdev_del(jng_cdev);
    cdev_del(jng_ctrl_cdev);
    printd("Distruggendo i device residui");
    device_destroy(jng_class, jng_ctrl_dev);
    for(i=0;i < 256;i++){
        if(J(i).dev) device_destroy(jng_class, MKDEV(MAJOR(jng_dev_base), i));
    }
    printd("Rimuovendo la regione di device");
    unregister_chrdev_region(jng_dev_base, 256);
    unregister_chrdev_region(jng_ctrl_dev, 1);
    printd("Cancellando la classe device");
    class_destroy(jng_class);
    printd("Fatto!");
}

module_init(jng_init);
module_exit(jng_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabio Meneghetti <fabiomene97@gmail.com>");
MODULE_DESCRIPTION("Permette di utilizzare l'interfaccia joystick-ng in user-level");
