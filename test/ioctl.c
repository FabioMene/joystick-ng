#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "../joystick-ng.h"

#define P(i) printf("%s: %ld\n", #i, i)

int main(){
    P(JNGIOCSETSLOT);
    P(JNGIOCGETSLOT);
    P(JNGIOCSETINFO);
    P(JNGIOCGETINFO);
    P(JNGIOCSETMODE);
    P(JNGIOCGETMODE);
    P(JNGIOCSETEVMASK);
    P(JNGIOCGETEVMASK);
    return 0;
}
