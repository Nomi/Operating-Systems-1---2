/*
 ------------------------------------------------------------------------
I declare that this piece of work which is the basis for recognition of
achieving learning outcomes in the OPS course was completed on my own.
Noman Noor 302343
------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
                     exit(EXIT_FAILURE))
int subchildpipefd;
volatile sig_atomic_t last_signal = 0;
int sethandler( void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1==sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}
void sig_handler(int sig) {
    last_signal = sig;
}

void sigchld_handler(int sig) {
    pid_t pid;
    for(;;){
        pid=waitpid(0, NULL, WNOHANG);
        if(0==pid) return;
        if(0>=pid) {
            if(ECHILD==errno) return;
            ERR("waitpid:");
        }
    }
}
void sigintSubchildHandler(int sig)
{
    if(sig==SIGINT)
    {
        if (close(subchildpipefd)) ERR("close in sigint handling");
        exit(EXIT_SUCCESS);
    }
};
void subchildwork(int PipeChildToSubChild,int n, int t,int b)    //work of child of child.
{
    subchildpipefd=PipeChildToSubChild;
    sethandler(sigintSubchildHandler,SIGINT);
    printf("subchild %d starting\n", getpid());
    sleep(1);
    srand(getpid());
/*   char c = 'a'+rand()%('z'-'a');
       while(n)
       {
           if(write(PipeChildToSubChild,&c,1) <0) ERR("Write to PipeChildToSubChild");
           c = 'a'+rand()%('z'-'a');
           n--;
           usleep(t*1000); //1000 microseconds = 1 milisecond
       } */
    char buff[PIPE_BUF - 6];
    for (int i = 0; i < n; i++)
    {
        memset(buff, 0, PIPE_BUF - 6);
        int length = abs(rand()%(PIPE_BUF - 6 +1 -b-8)) + b;   //b is min and PIPE_BUF is mas len   //we subtract 8 because we might append 8 bytes in the child
        for(int j = 0; j < length; j++)
        {
            buff[j] = 'a' + rand()%('z'-'a');
        }
        if(write(PipeChildToSubChild, buff, PIPE_BUF - 6-8) < 0) ERR("write to PipeChildToSubChild in SubChild");   //we subtract 8 because we might append 8 bytes in the child
        usleep(t*1000);
    }
    if (close(PipeChildToSubChild)) ERR("close");
    printf("subchild %d exiting\n", getpid());
    exit(EXIT_SUCCESS);
}
void child_work(int PipeParentToChild, int n, int t, int b,int r)
{
    printf("child %d starting:\n", getpid());
    int PipeChildToSubChild[2];
    if (pipe(PipeChildToSubChild)) ERR("pipe");
    int d = 1;
    while (d)
    {
        switch (fork())
        {
            case 0:
                subchildwork(PipeChildToSubChild[1], n, t, b);
                break;
            case -1:
                ERR("Fork:");
        }
        d--;
    }
    //implement actual work here
    if (close(PipeChildToSubChild[1])) ERR("close");   //was M[0] earlier, which is dumb.
    /*   int status; char c;
       while((status=read(PipeChildToSubChild[0],&c,1))==1)
       {
           if(write(PipeParentToChild,&c,1) <0) ERR("write to PipeParentToChild");
           if(status==0)
           {
               printf("child %d exiting\n", getpid());
               while(wait(0)){}
               exit(EXIT_SUCCESS); //makes sure that if the write end of pipe is closed, this process closes.
           }
       }
       if(status<0) ERR("read from PipeParentToChild"); */
    int readsize;
    srand(getpid());
    char buff[PIPE_BUF - 6];
    do {
        if ((readsize = read(PipeChildToSubChild[0], buff, PIPE_BUF - 6)) < 0) ERR("Read from PipeChildToSubChild error in Child");
        if (readsize > 0)
        {
            double prob=(double)rand()/RAND_MAX;
            if(prob<=r/100)  // because we want r% probability i.e. prob< r/100
            {
                int count;
                for(count=0;buff[count]!=0;count++)
                {};
                for(int i=0;i<8;i++)
                {

                    buff[count+i]= 'a'+rand()%('z'-'a');
                }
                if (write(PipeParentToChild, buff, PIPE_BUF - 6) < 0) ERR("APPENDED Write to PipeParentToChild error in Child");
            }
            else
            {
                if (write(PipeParentToChild, buff, PIPE_BUF - 6) < 0) ERR("Write to PipeParentToChild error in Child");
            }
        }
    } while (readsize > 0);
    if(readsize<0) ERR("read failure");
    if (close(PipeChildToSubChild[0])) ERR("Closing PipeChildToSubChild error in Child");
    if (close(PipeParentToChild)) ERR("close");
    wait(NULL);
    printf("Child %d exiting\n", getpid());
    exit(EXIT_SUCCESS);
}

void parent_work(int PipeParentToChild)
{
    /*   char c;
       int status;
       srand(getpid());
       while((status=read(PipeParentToChild,&c,1))==1) {
           printf("%c", c);
           if(status==0)
           {
               exit(EXIT_SUCCESS); //makes sure that if the write end of pipe is closed, this process closes.
           }
       }
       if(status<0) ERR("read from PipeParentToChild");
       printf("\n");
       while(wait(NULL) > 0){}
       printf("Parent PID: %d\n", getpid()); */

    char buff[PIPE_BUF-6];
    int readcall=0;
    int status;
    size_t bytesRead;
    srand(getpid());
    while((status=read(PipeParentToChild,buff,PIPE_BUF-6))>0)
    {
        bytesRead=strlen(buff);
        ++readcall;
        printf("[%d]: [%d]: [", readcall,bytesRead);
//        for(int i = 0; i < bytesRead; i++)      //i<bytesRead because bytesRead=NumberOfCharactersRead
        for(int i=0;i<bytesRead;i++)
        {
            printf("%c", buff[i]);
        }
        printf("]\n");
    } if(status<0) ERR("read from PipeParentToChild");
    //   printf("\n");
    if(status==0)
    {
        while(wait(NULL) > 0){}
        printf("Parent PID: %d [EXIT]\n", getpid());
        exit(EXIT_SUCCESS); //makes sure that if the write end of pipe is closed, this process closes.
    }
}

void create_children_with_pipes(int d, int PipeParentToChild,int n, int t,int b,int r)
{

    int maxChildren=d;
    while (maxChildren) {
        switch (fork()) {
            case 0:
                child_work(PipeParentToChild,n,t,b,r);
                break;
            case -1: ERR("Fork:");
        }
        maxChildren--;
    }
}

int main(int argc, char* argv[])
{
    /*intentional memory leak for testing valgrind:*/	//int lek=100; char* leaker=malloc(sizeof(char)*lek);
    //arguments\parameters handling:
    if (argc <5)    //program name + 4  more arguments
    {
        printf("Incorrect number of arguments.\n");          //Replace with proper error stuff
    }
    int t=-1; int n=-1; int r =-1; int b=-1; //-t,-n,-r
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
        {
            t = atoi(argv[i + 1]);
            if(t<50||t>500)
            {
                printf("Invalid arguments.\n");
                return -1;
            }
            i += 1;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            n = atoi(argv[i + 1]);
            if(n<3||n>30)
            {
                printf("Invalid arguments.\n");
                return -1;
            }
            i += 1;
        }
        else if (strcmp(argv[i], "-r") == 0)    //was -i earlier.
        {
            r = atoi(argv[i + 1]);
            if(r<0||r>100)
            {
                printf("Invalid arguments.\n");
                return -1;
            }
            i += 1;
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            b = atoi(argv[i + 1]);
            if(b<1||b>PIPE_BUF-6)
            {
                printf("Invalid arguments.\n");
                return -1;
            }
            i += 1;
        }
    }
    if(t==-1||r==-1||r==-1||b==-1)
    {
        printf("Invalid arguments.\n");
        return -2;
    }
    if(sethandler(SIG_IGN,SIGINT)) ERR("Setting SIGINT handler");
    if(sethandler(SIG_IGN,SIGPIPE)) ERR("Setting SIGPIPE handler");
    //   if(sethandler(SIG_IGN,SIGCHLD)) ERR("Setting SIGCHLD handler");
    printf("Parent PID: %d [START]\n", getpid());
    int PipeParentToChild[2];
    if(pipe(PipeParentToChild)) ERR("pipe");
//    if(NULL==(fds=(int*)malloc(sizeof(int)*2))) ERR("malloc");      //*n->*2-?>*1
    create_children_with_pipes(2,PipeParentToChild[1],n,t,b,r);
    if(close(PipeParentToChild[1])) ERR("close");
    parent_work(PipeParentToChild[0]);
    if(close(PipeParentToChild[0])) ERR("close");
    return 0;
}
