/*
 * joystick-ng-core.c
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
 */

// PREMESSA
// In questo progetto il termine "connessione" indica un file (character device per la precisione) aperto
// e tutti i dati vengono passati da kernel a userspace e viceversa tramite questo file
// Inoltre "device", "client", "driver" e "device driver" si riferiscono a 4 cose completamente diverse
// device: l'effettivo file in /dev/jng, aprirne uno crea una connessione tra applicazione e kernel (per scopi diversi) 
// client: chi si connette a /dev/jng/device (lo so, ma si chiama proprio device) per RICEVERE i dati di un joystick, dal kernel al client
// driver: chi si connette a /dev/jng/driver per INVIARE i dati di un joystick, dal -appunto- driver (usb/hid/bluetooth/stregoneria) al kernel
// device driver: l'insieme di funzioni che definiscono le operazioni eseguibili su una connessione (le funzioni in struct file_operations), nel kernel (joystick-ng-{driver,client}-fops.c)
// Spero di avervi confuso abbastanza ancora prima di iniziare

// Ricordo inoltre che nessuna operazione blocca (se non per il tempo necessario all'acquisizione degli spinlock)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>

// Include anche joystick-ng.h
#include "joystick-ng-core.h"

// I due device, uno per i driver e uno per i client
static dev_t       drivers_dev;
static struct cdev drivers_cdev;

static dev_t       clients_dev;
static struct cdev clients_cdev;

// La classe in sysfs
static struct class* jng_class;

// I joystick. Non, non c'è molto da spiegare
jng_joystick_t jng_joysticks[JNG_TOT];
DEFINE_SPINLOCK(jng_joysticks_lock);

// Inizializzazione
static int __init jng_init(void){
    #define _init_fail(op, lbl) do{printe("Errore inizializzazione driver: " # op);goto init_ ## lbl ## _fail;}while(0)
    
    // Inizalizzazione strutture
    int i;
    for(i = 0;i < JNG_TOT;i++){
        jng_joysticks[i].num = i;
        jng_joysticks[i].driven = 0;
        rwlock_init(&jng_joysticks[i].state_lock);
        spin_lock_init(&jng_joysticks[i].feedback_lock);
        
        init_waitqueue_head(&jng_joysticks[i].state_queue);
        jng_joysticks[i].state_inc = 0;
        init_waitqueue_head(&jng_joysticks[i].feedback_queue);
        jng_joysticks[i].feedback_inc = 0;
    }
    
    // Creazione devices (il primo viene messo nel primo argomento, il secondo va calcolato)
    if(alloc_chrdev_region(&drivers_dev, 0, 2, JNG_DRIVER_NAME) < 0) _init_fail(alloc_chrdev_region, );
    clients_dev = MKDEV(MAJOR(drivers_dev), 1);

    // Creazione classe
    jng_class = class_create(THIS_MODULE, JNG_DRIVER_NAME);
    if(!jng_class) _init_fail(class_create, class);
    
    // Creazione device per i driver
    if(device_create(jng_class, NULL, drivers_dev, NULL, "jng/" JNG_DRIVERS_DEVICE_NAME) < 0) _init_fail(device_create drivers, device_drivers);
    
    cdev_init(&drivers_cdev, &joystick_ng_driver_fops);
    
    if(cdev_add(&drivers_cdev, drivers_dev, 1) < 0) _init_fail(cdev_add drivers, add_drivers);
    
    // Creazione device per i client
    if(device_create(jng_class, NULL, clients_dev, NULL, "jng/" JNG_CLIENTS_DEVICE_NAME) < 0) _init_fail(device_create clients, device_clients);
    
    cdev_init(&clients_cdev, &joystick_ng_client_fops);
    
    if(cdev_add(&clients_cdev, clients_dev, 1) < 0) _init_fail(cdev_add clients, add_clients);
    printi("Inizializzato");
    return 0;
    
    // Qui finiscono i vari fail da sopra, in ordine inverso
  init_add_clients_fail:
    cdev_del(&clients_cdev);
    device_destroy(jng_class, clients_dev);
    
  init_device_clients_fail:
  init_add_drivers_fail:
    cdev_del(&drivers_cdev);
    device_destroy(jng_class, drivers_dev);
    
  init_device_drivers_fail:
    class_destroy(jng_class);
  
  init_class_fail:
    unregister_chrdev_region(drivers_dev, 2);
  
  init__fail:
    printe("Errore inizializzazione driver");
    return 1;
    
    #undef _init_fail
}
module_init(jng_init);


// Cleanup
static void __exit jng_exit(void){
    cdev_del(&clients_cdev);
    device_destroy(jng_class, clients_dev);
    
    cdev_del(&drivers_cdev);
    device_destroy(jng_class, drivers_dev);
    
    class_destroy(jng_class);
    
    unregister_chrdev_region(drivers_dev, 2);
    
    printi("Rimosso");
}
module_exit(jng_exit);


// Roba
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabio Meneghetti <fabiomene97@gmail.com>");
MODULE_DESCRIPTION("joystick-ng permette un controllo migliore sui joystick rispetto al metodo standard (input, hid, etc.)");


