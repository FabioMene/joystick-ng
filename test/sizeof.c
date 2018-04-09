#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "../joystick-ng.h"

#define P(i) printf("%s: %ld\n", #i, sizeof(i))

int main(){
    P(jng_state_t);
    P(jng_event_t);
    P(jng_feedback_t);
    P(jng_info_t);
    return 0;
}
