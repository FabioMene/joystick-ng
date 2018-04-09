#include <stdio.h>
#include <stdlib.h>
#include "../queue.h"


void print_queue(queue_t* q){
    int i;
    int n;
    
    if(q->buflen == 0){
        printf("Coda vuota\n");
        return;
    }
    
    for(i = 0;i < q->buflen;i++){
        printf("|% 4d", *((int*)(q->buffer + i * q->element_len)));
    }
    printf("|\n");
}

int getint(char* str){
    int i;
    sscanf(str + 2, "%i", &i);
    return i;
}

int evencb(void* el, void* arg){
    return 1 - *((int*)el) % 2;
}

int oddcb(void* el, void* arg){
    return *((int*)el) % 2;
}

int main(){
    queue_t q;
    q.buflen = 0;
    
    char cmd[32];
    
    printf("i: init\n+: add\np: pop\ng: get\nd: del\n0: del pari\n1: del dispari\na: del tutto\n");
    
    while(1){
        print_queue(&q);
        printf("> ");
        fflush(stdout);
        fgets(cmd, 32, stdin);
        switch(cmd[0]){
            case 'i':
                queue_init(&q, sizeof(int), 10);
                break;
            case '+': {
                int d = getint(cmd);
                queue_add(&q, &d);
            }; break;
            case 'p': {
                int d;
                queue_pop(&q, &d);
                printf("pop: %d\n", d);
            }; break;
            case 'g': {
                int d;
                queue_get(&q, &d, getint(cmd));
                printf("get: %d\n", d);
            }; break;
            case 'd':
                queue_del(&q, getint(cmd));
                break;
            case '0':
                queue_delcb(&q, evencb, NULL);
                break;
            case '1':
                queue_delcb(&q, oddcb, NULL);
                break;
            case 'a':
                queue_delall(&q);
                break;
            case 'q':
                return 0;
        }
    }
}

