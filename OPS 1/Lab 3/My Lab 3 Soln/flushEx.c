#include <stdio.h>
#include <unistd.h>

int main(void)
{
    fprintf(stdout,"wow");
    fprintf(stdout,"newl\n");
    sleep(2);
    for(int i=0;i<10;i++)
    {
        printf("%d",i);
        fflush(stdout);
        usleep(100);
    }
    sleep(1);
    fprintf(stdout,"wow");
    fflush(stdout);
    sleep(30);

    return 0;
}