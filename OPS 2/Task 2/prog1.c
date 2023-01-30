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
//#include <fnmatch.h>

#define OUTPUT
#define notDEBUG
#define notMOREOUTPUT


#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
                                     exit(EXIT_FAILURE))
volatile sig_atomic_t last_signal;

typedef struct _statusMsg
{
    int pidSender;
    int status;
//    int isStatusMessage;
} statusMsg;


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

int isRegisterMessage(char* msg)
{
    int msglen=strlen(msg);
    char reg[]={'R','E','G','I','S','T','E','R',' '};
    int reglen=strlen(reg);
    if(msglen<reglen)
    {
        return -1;  //-1 means the legnth was lesser
    }
    int strpos=0;
    for(;strpos<msglen&&strpos<reglen&&reg[strpos]!='\0';strpos++)  // probably &&reg[strpos]!='\0' wasn't needed because strlen doesn't count \0
    {
        if(reg[strpos]!=msg[strpos])
            return -2;  //-2 means it didn't have "REGISTER " in the beginning.
    }
    char* pidMsgStr= malloc(sizeof(char)*(msglen-reglen+1)); //we add +1 to store the '\0';
    for(int i=0;strpos<=msglen;strpos++,i++) //<= instead of just < because we need to copy '\0'
    {
        pidMsgStr[i]=msg[strpos];
    }
    int result=atoi(pidMsgStr);
    free(pidMsgStr);
    return result;
}

int isStatusMessage(char* msgOriginal,statusMsg* msgStore)
{
    char* msg=strdup(msgOriginal);
    int msglen=strlen(msg);
    char stat[]={'S','T','A','T','U','S',' '};
    int statlen=strlen(stat);
    if(msglen<statlen)
    {
        return -1;  //-1 means the legnth was lesser
    }
    int strpos=0;
    for(;strpos<statlen;strpos++)  // probably &&reg[strpos]!='\0' wasn't needed because strlen doesn't count \0
    {
        if(stat[strpos]!=msg[strpos])
            return -2;  //-2 means it didn't have "STATUS " in the beginning.
    }
    /*char* pidMsgStr= malloc(sizeof(char)*(msglen-statlen-1)); //we subtract 1 because there are ' ' and 'int status' there, and we need one space out of those for \0 and don't need the other;
    int i=0;
    for(;msg[strpos]!=' ';strpos++,i++) //<= instead of just < because we need to copy '\0'
    {
        pidMsgStr[i]=msg[strpos];
        fprintf(stdout,"%d",msg[strpos]);
    }*/
    ////



    const char s[2] = " ";
    char *token;

    /* get the first token */
    token = strtok(msg, s);
    token = strtok(NULL,s);
    if(token==NULL)
        return-3;
    else
        msgStore->pidSender=atoi(token);
    token =strtok(NULL,s);
    if(token==NULL)
        return-4;
    else
        msgStore->status=atoi(token);




    ////
//    pidMsgStr[i]='\0';
//    int status=msg[strpos+1];
//    msgStore->status=status;
//   msgStore->pidSender=atoi(pidMsgStr);
    free(msg);
    return 1;
}

void usage(void){
    fprintf(stderr,"USAGE: q0_name t \n");
//    fprintf(stderr,"USAGE: q0_name\n");
    fprintf(stderr,"2000 >=t >= 100 - \n");
    exit(EXIT_FAILURE);
}

void child_work(char* qPIDname, int t) {
    mqd_t qPID;
    struct mq_attr attr; //attr.mq_flags=O_EXCL|O_RDWR|O_CREAT;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 100;
    attr.mq_curmsgs = 0;
    char *my_snd = strdup("CHECK STATUS");
    if ((qPID = TEMP_FAILURE_RETRY(mq_open(qPIDname, O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t) -1)ERR("mq open");
#ifdef DEBUG
    fprintf(stdout,"Opened queue with name: %s\n",qPIDname);
#endif
//    if (TEMP_FAILURE_RETRY(mq_send(qPID, (char *) my_snd, strlen(my_snd) + 1, 2)))ERR("mq_send");
    for(;;) {
        usleep(t*1000);
        if (last_signal == SIGINT) {
            break;
        }
        if (TEMP_FAILURE_RETRY(mq_send(qPID, (char *) my_snd, strlen(my_snd) + 1, 2))) {
            if(errno==EINTR)
            {
                last_signal=SIGINT;
                break;
            }
//            else if(errno==ETIMEDOUT)
//            {
 //               break;
//            }
            else
                ERR("mq_send: ");
        }
    }
    free(my_snd);
    mq_close(qPID);
    mq_unlink(qPIDname);
}

void create_child(char* qPIDname, int t) {

        switch (fork()) {
            case 0:
            {
                char*qPIDnameStore=strdup(qPIDname); //created to avoid destruction of it.
#ifdef DEBUG
                fprintf(stdout,"Creating child and sent arguments %s,%d\n",qPIDnameStore,t);
#endif
                child_work(qPIDnameStore,t);
                free(qPIDnameStore);
                exit(EXIT_SUCCESS);
            }
                break;
            case -1:
            {
                perror("Fork:");
                exit(EXIT_FAILURE);
            }
                break;
        }
}

void main(int argc, char** argv) {
    if(argc!=3) usage();
    int t = atoi(argv[2]);
    if (t<100||t>2000)  usage();
    srand(time(NULL));

    //signal handling config starts
    sethandler(sig_handler, SIGINT);
    //signal handling config ends


    mqd_t q0;
    struct mq_attr attr; //attr.mq_flags=O_EXCL|O_RDWR|O_CREAT;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 100;
    attr.mq_curmsgs = 0;
    if((q0=TEMP_FAILURE_RETRY(mq_open(argv[1], O_RDWR| O_CREAT, 0600, &attr)))==(mqd_t)-1)ERR("mq open in prog1");
//    uint8_t my_msg=(uint8_t)(rand())%2; //0 or 1
//    if(TEMP_FAILURE_RETRY(mq_send(q0,(const char*)&my_msg,1,1)))ERR("mq_send");
//    printf("message sent: %d\n",my_msg);
    char* my_rcv;
    unsigned msg_prio;
    int msg_size=attr.mq_maxmsg*attr.mq_maxmsg;
    my_rcv=malloc(msg_size);
    statusMsg statusRecv;
    for(;;){
        if(last_signal==SIGINT)
        {
            break;
        }
        if(mq_receive(q0,(char*)my_rcv,msg_size,&msg_prio)<1)
        {
            if(errno==EINTR)
                break;
            else ERR("mq_receive");
        }
        else
        {
#ifdef DEBUG
            fprintf(stdout,"Contents: %s\n",my_rcv);
            fprintf(stdout,"Checking what type of message was received\n");
#endif
            int pid2register;
            if((pid2register=isRegisterMessage(my_rcv))>=0)
            {
#ifdef MOREOUTPUT
                fprintf(stdout,"Register message from pid=%d with contents: %s\n",pid2register,my_rcv);
#endif
                char qPIDname[100];
                sprintf(qPIDname,"/q%d",pid2register);
                create_child(qPIDname,t);
            }
            else if(isStatusMessage(my_rcv,&statusRecv)==1)
            {
#ifdef OUTPUT
                fprintf(stdout,"Status Message received from process: %d, with status %d.\n",statusRecv.pidSender,statusRecv.status);
#endif
            }
            else
            {
#ifdef DEBUG
               fprintf(stdout,"The message received is not a registration OR status message.\n");
#endif
            }

//            if(fnmatch("register +([0-9])",my_rcv,FNM_EXTMATCH))
//            {
//                printf("contents: %s\n",my_rcv);
//            }
//            printf("contents: %s\n",my_rcv);
//            break;
        }
    }
    fflush(stdout);
    free(my_rcv);
    wait(NULL);
    mq_close(q0);
    mq_unlink(argv[1]);
    exit(EXIT_SUCCESS);
}