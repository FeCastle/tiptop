#include <pthread.h>
#include <stdio.h>

/* A messy program, that spawns N threads and dies. */

#define N 9
#define M 100000

void *threadFunc()
{
    int i = 0;
    for(i=0;i<M;i++) usleep(10);
    return NULL;
}

int main(void)
{
    int i = 0;
    
    pthread_t pth;
    for(i=0;i<N;i++) pthread_create(&pth,NULL,threadFunc,NULL);
    pthread_join(pth,NULL);

    return 0;
}