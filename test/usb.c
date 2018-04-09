#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <usb.h>

int main(){
    usb_init();
    usb_find_busses();
    usb_find_devices();
    struct usb_bus* bus = usb_get_busses();
    while(bus){
        printf("bus %d (%s)\n", bus->location, bus->dirname);
        struct usb_device* dev = bus->devices;
        while(dev){
            printf("  dev %d (%s) %04x/%04x\n", dev->devnum, dev->filename, dev->descriptor.idVendor, dev->descriptor.idProduct);
            dev = dev->next;
        }
        bus = bus->next;
    }
}
