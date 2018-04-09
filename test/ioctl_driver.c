#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "../joystick-ng.h"

jng_info_t info;

#define tioctl(cmd, arg) do{errno = 0;int r = ioctl(fd, cmd, arg);printf("ioctl(" #cmd ", " #arg ") = %d (%s)\n", r, strerror(errno));}while(0)

int main(){
    printf("Test ioctl driver\n");

    int fd = open("/dev/jng/driver", O_RDWR);

    int arg;

    tioctl(JNGIOCSETSLOT, 2);
    tioctl(JNGIOCGETSLOT, &arg);
    printf("%d\n", arg);

    info.keys = JNG_KEY_A | JNG_KEY_R2;
    tioctl(JNGIOCSETINFO, &info);
    info.keys = 0;
    tioctl(JNGIOCGETINFO, &info);
    printf("%d\n", info.keys);

    tioctl(JNGIOCSETMODE, JNG_MODE_EVENT);
    tioctl(JNGIOCSETMODE, &arg);
    printf("%d\n", arg);

    tioctl(JNGIOCSETEVMASK, JNG_EV_KEY | JNG_EV_FEEDBACK);
    tioctl(JNGIOCGETEVMASK, &arg);
    printf("%d\n", arg);

    close(fd);

    return 0;
}
