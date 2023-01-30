/*
 ------------------------------------------------------------------------
I declare that this piece of work which is the basis for recognition of
achieving learning outcomes in the OPS course was completed on my own.
Noman Noor 302343
------------------------------------------------------------------------
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <mqueue.h>

#define notDEBUG
#define notMOREOUTPUT

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
                                     exit(EXIT_FAILURE))
volatile sig_atomic_t last_signal;

void sethandler( void (*f)(int, siginfo_t*, void*), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags=SA_SIGINFO;
    if (-1==sigaction(sigNo, &act, NULL)) ERR("sigaction");
}
void sig_handler(int sig) {
    last_signal = sig;
}

/*char* strAllocCat(char* str1,char*str2)
{
    char*temp= malloc(sizeof(char*)*strlen(strcat(str1,str2)-1));
    int t=strlen(str1)-1;
    for(int i=0;i<strlen(strcat(str1,str2)-1));i++)
    {
    if(i<strlen(str1)-1)
    {
    if(i==strlen(str1)-2)
    {
        continue;
    }
    temp[i]=str1[i];
    }
    else if(i<strlen(str2)-1)
    {
    temp[i-t]
    }
    }
}*/




void usage(void){
    fprintf(stderr,"USAGE: q0_name t \n");
//    fprintf(stderr,"USAGE: q0_name\n");
    fprintf(stderr,"2000 >=t >= 100 - \n");
    exit(EXIT_FAILURE);
}

void main(int argc, char** argv) {
    if(argc!=3) usage();
    int t = atoi(argv[2]);
    if (t<100||t>2000) usage();
    srand(getpid());
    sethandler(sig_handler, SIGINT);
    mqd_t q0;
    struct mq_attr attr; //attr.mq_flags=O_EXCL|O_RDWR|O_CREAT;
    attr.mq_maxmsg=10;
    attr.mq_msgsize=100;
    char my_snd[100];
    snprintf(my_snd,sizeof(my_snd)-strlen(my_snd),"REGISTER %d",getpid());
//    char* my_snd=strdup("reg");
/*    if((q0=TEMP_FAILURE_RETRY(mq_open(argv[1], O_RDWR|O_EXCL | O_CREAT, 0600, &attr)))!=(mqd_t)-1)
    {
        mq_close(q0);
        mq_unlink(argv[1]);
        ERR("such a queue doesn't exist");
    }*/
    if((q0=TEMP_FAILURE_RETRY(mq_open(argv[1], O_RDWR, 0600, &attr)))==(mqd_t)-1){
        if(errno==ENOENT)
        {
            ERR("The given q0 queue doesn't exist.\n mq_open: ");
        }
        ERR("mq open");
    }
    if(TEMP_FAILURE_RETRY(mq_send(q0,(char*)my_snd,strlen(my_snd)+1,1)))ERR("mq_send");
#ifdef MOREOUTPUT
    printf("REGISTER message sent: %s\n",my_snd);
#endif
    //    free(my_snd);

    // RECEIVING PART:

    char qPIDname[100];
    sprintf(qPIDname,"/q%d",getpid());
    fflush(stdout);
    mqd_t qPID;
    for(;;)
    {
        if((TEMP_FAILURE_RETRY(qPID=mq_open(qPIDname, O_RDWR|O_NONBLOCK, 0600, &attr)))==(mqd_t)-1)
        {
            if(errno==ENOENT)
                continue;
            else
                ERR("mq open");
        }
        else
        {
            break;
        }
    }
    unsigned msg_prio;
    int msg_size=attr.mq_maxmsg*attr.mq_maxmsg;
    char* my_rcv;
    my_rcv=malloc(msg_size);
    int status=rand()%2;
    for(;;)
    {
        if(last_signal==SIGINT)
        {
            break;
        }
        if(mq_receive(qPID,(char*)my_rcv,msg_size,&msg_prio)<1)
        {
            if(errno==EAGAIN)
            {
#ifdef MOREOUTPUT
                fprintf(stdout,"STATUS CHECK message not recieved. Falling back to Randomization of Status.\n");
#endif
                status=rand()%2;
                usleep(t*1000);
                continue;
            }
            else if(errno==EINTR)
                break;
            else if(errno==ETIMEDOUT)
            {
#ifdef DEBUG
                printf("Wait for CHECK STATUS message timed out.");
#endif
                continue;
            }
            else
                ERR("mq_timedreceive");
        }
        else
        {
#ifdef MOREOUTPUT
            printf("CHECK STATUS message recieved: %s.\nNew random status value calculated=%d.\n", my_rcv,status);
#endif
            char my_rspns[100];
            snprintf(my_rspns,100,"STATUS %d %d",getpid(),status);
            if(TEMP_FAILURE_RETRY(mq_send(q0,(char*)my_rspns,strlen(my_rspns)+1,3)))ERR("mq_send");
            status=rand()%2;    //I call rand here as well because if somehow the times on both prog1 and prog2 sync up perfectly, then it'd never go to EAGAIN to randomize.
            continue;
        }
    }
/*    for(;;){
        if(mq_receive(q0,(char*)my_rcv,msg_size,&msg_prio)<1)
        {
            if(errno==EAGAIN) continue; //only error we can accept
            else ERR("mq_receive");
        }
        else
        {
            int pid2register;
            if((pid2register=isRegisterMessage(my_rcv))>=0)
            {
                char* qPIDname[100];
                sprintf(qPIDname,"/q%d",pid2register);
//ifdef    printf("From pid=%d with contents: %s : New queue name=%s\n",pid2register,my_rcv,qPIDname);
 //endif
            }
            else
            {
                //the following is commented because we have to ignore any other messages //printf("Message is not a registration message\n");
            }

//            if(fnmatch("register +([0-9])",my_rcv,FNM_EXTMATCH))
//            {
//                printf("contents: %s\n",my_rcv);
//            }
//            printf("contents: %s\n",my_rcv);
//            break;
        }
        if(last_signal==SIGINT)
        {
            break;
        }
    }*/
    wait(NULL);
    free(my_rcv);
    mq_close(q0);
    mq_close(qPIDname);
    exit(EXIT_SUCCESS);
}