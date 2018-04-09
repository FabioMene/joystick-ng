#include <stdio.h>
#include <libjngdsett.h>

int main(int argc, char* argv[]){
    int num;
    
    jngdsett_load("ds3");
    
    jngdsett_opt_t* opt = jngdsett_optdata(&num);
    
    int i;
    for(i = 0;i < num;i++){
        printf("'%s' %d '%s' '%s'\n", opt[i].name, opt[i].type, opt[i].value, opt[i].description);
    }
    
    int n;
    jngdsett_read("mac_src", &n);
    printf("mac_src: %d\n", n);
    
    jngdsett_write("mac_src", "2");
    
    jngdsett_write("master_mac", "34:68:95:76:24:D2");
    
    return 0;
}
