#define _GNU_SOURCE
///IMPORTANT NOTE:
//The essence is the following: Signal handlers are per-process, but signal masks are per-thread.

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <signal.h>



#define Sec2Mili *1000
#define Mili2Micro *1000



//WEIRD FUNCTION FOR FUN:
//For color "codes": https://stackoverflow.com/questions/17771287/python-octal-escape-character-033-from-a-dictionary-value-translates-in-a-prin
void red () {
  printf("\033[1;31m");
}

void yellow() {
  printf("\033[1;33m");
}

// void green() {
//   printf("\033[1,32m");
// }

// void blue() {
//   printf("\033[1,34m");
// }

void reset () {
  printf("\033[0m");
}

//MACROS:
#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source), exit(EXIT_FAILURE))

///typedef and STRUCTS:
typedef struct THREADARGS { 
    pthread_t tid;
    // int indexANYC;
    // int* arrNYC;
    int* toIncrement;
    uint seed;
    pthread_mutex_t* mtxCurrArrPosBeingRead;
} threadArgs;


typedef struct SIGPTARG { 
    pthread_t tid;
    int* sigIntRecvd;
    sigset_t *pMask;
    pthread_mutex_t* mtxSigIntRecv;
} sigThreadArg;
///GLOBAL VARS:




///non-ready-made FUNCTION DEFINITIONS:

void* threadWork(void *voidPtr);
void* sigThreadWork(void *voidPtr);
void getCmdArgs(int* , int* , const int, char**); ///could label input arguments here but the labels are REQUIRED only on definition. However, IDK if it affects helpfulness of hint features of IDEs,etc.
void setupSigHandlingThread(sigThreadArg* sigThr, int* sigIntFlag,pthread_mutex_t* UNINITIALIZEDmtxSIGINTRecv, sigset_t* newMask);
void InitializeNYC_NoMtx(const int N, int* arrNYC);
void printArrNYC(int N, int* arrNYC, pthread_mutex_t** arr_mtxArrayBeingRead) ;
void resetAndPrintArrNYC(int N, int* arrNYC, pthread_mutex_t** arr_mtxArrayBeingRead); 
void createBuildingThreads(const int N, threadArgs* tArgs, int* arrNYC, pthread_mutex_t** arr_mtxArrayBeingRead);
void CancelAndJoinThreads(const int N, threadArgs* tArgs);
int KingKongWork(const int N, const int T, sigThreadArg* pSigThr, pthread_mutex_t** mtxArrayBeingRead, int* arrNYC);


///IMPLEMENTATIONS OF READY-MADE (or only slightly specific) FUNCTIONS :

void usage(char *name){
	fprintf(stderr,"USAGE: %s N T \n",name);
	exit(EXIT_FAILURE);
}


//MAIN FUNCTION:
int main(int argc, char** argv)
{
    srand(time(NULL));

    //Arguments:
    int N, T;
    getCmdArgs(&N,&T,argc,argv);

    //Setting up a thread for signal handling.
    sigThreadArg sigThr; int sigIntRecieved=0;  sigset_t newMask;
    pthread_mutex_t mtxSIGINTRecv = PTHREAD_MUTEX_INITIALIZER;
    setupSigHandlingThread(&sigThr,&sigIntRecieved,&mtxSIGINTRecv, &newMask);
    
    //Initializing Simulation Variables:
    int* arrNYC=(int*) malloc(N * sizeof(int));
    InitializeNYC_NoMtx(N, arrNYC); //No mutexes needed as no threads created for now.

    //Dealing with Thread Creation
    pthread_mutex_t** mtxArrayBeingRead = malloc(sizeof(pthread_mutex_t*)*N); //just adding address of temp in the next for loop doesn't work. IDK why tho.
    pthread_mutex_t* WWmtxArrayBeingRead = malloc(sizeof(pthread_mutex_t)*N);
    for(int i=0;i<N;i++)
    {
        pthread_mutex_t temp = PTHREAD_MUTEX_INITIALIZER;
        //mtxArrayBeingRead[i]=&temp; 
        WWmtxArrayBeingRead[i] = temp;
        mtxArrayBeingRead[i]= &WWmtxArrayBeingRead[i];
        // printf("(size of mutex=%d)TEMP Creating addresses: %d\n",sizeof(pthread_mutex_t),&WWmtxArrayBeingRead[i]);
    }
    threadArgs* tArgs = (threadArgs*) malloc(sizeof(threadArgs) * N);
    createBuildingThreads(N,tArgs,arrNYC,mtxArrayBeingRead);
    //KingKong (Main thread) simulation:
    int kkFinalIndex= KingKongWork(N,T,&sigThr,mtxArrayBeingRead, arrNYC);

    //Cancelling and joining threads
    CancelAndJoinThreads(N, tArgs);
    pthread_cancel(sigThr.tid);
    int err = pthread_join(sigThr.tid,NULL); //pthread cancel directly followed by join prevents trying to access data parent has freed already.
    if (err != 0){ errno = err; ERR("Can't join with a thread");}

    //Printing Final Info:
    yellow();
    printf("Final KKpos (Index)= %d ; Array: ",kkFinalIndex);
    printArrNYC(N,arrNYC,mtxArrayBeingRead);
    reset();
    printf("\n"); //fflushes 

    //Freeing resources:
    pthread_mutex_destroy(&mtxSIGINTRecv);
    free(tArgs);
    free(arrNYC);
    for(int i=0;i<N;i++)
    {
        pthread_mutex_destroy(mtxArrayBeingRead[i]);
    }
    free(mtxArrayBeingRead);
    free(WWmtxArrayBeingRead);
    //Returning:
    return 0;
}



//IMPLEMENTATIONS OF SPECFIC FUNCTIONS.

void* threadWork(void *voidPtr)
{
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    threadArgs* Arg = voidPtr;
    // printf("Created.\n");
    while(1)
    {

        int Time2Sleep=(rand_r(&(Arg->seed))%501+100) Mili2Micro;
        usleep(Time2Sleep);
        pthread_mutex_lock(Arg->mtxCurrArrPosBeingRead); //no need to use cleanup, no cancellation point between lock and unlock.
        // Arg->arrNYC[Arg->indexANYC]++;
        *(Arg->toIncrement)+=1;
        pthread_mutex_unlock(Arg->mtxCurrArrPosBeingRead);
    }
}

void* sigThreadWork(void *voidPtr)
{
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    sigThreadArg* sarg=voidPtr;
    int signo;
    for (;;) 
    {
        if(sigwait(sarg->pMask, &signo)) ERR("sigwait failed.");//no need for cleanup functions as no mutex is locked here (neither is there any mallocd thing).
        switch (signo) {
                case SIGINT:
                        pthread_mutex_lock(sarg->mtxSigIntRecv);
                        *sarg->sigIntRecvd=1;
                        pthread_mutex_unlock(sarg->mtxSigIntRecv);
                        break;
                default:
                        printf("unexpected signal %d\n", signo);
                        exit(1);
        }
    }
}




void getCmdArgs(int* ptrN, int* ptrT, const int argc, char** argv)
{
    int N,T;
    if(argc!=3)
    {
        usage(argv[0]);
    }
    else 
    {
        N=atoi(argv[1]);
        T=atoi(argv[2]);
        if(N<=2||T<5)
        {
            usage(argv[0]);
        }
    }
    *ptrN=N;
    *ptrT=T;
}

void setupSigHandlingThread(sigThreadArg* sigThr, int* sigIntFlag,pthread_mutex_t* INITIALIZED_mtxSIGINTRecv, sigset_t* newMaskAddr)
{
    //Addr=address (i.e. pointer).
    sigset_t oldMask;
    sigemptyset(newMaskAddr);
    sigaddset(newMaskAddr, SIGINT);
    
    if (pthread_sigmask(SIG_BLOCK, newMaskAddr, &oldMask)) ERR("SIG_BLOCK error");

    sigThr->pMask=newMaskAddr;
    sigThr->sigIntRecvd = sigIntFlag;
    sigThr->mtxSigIntRecv=INITIALIZED_mtxSIGINTRecv;
    int err = pthread_create(&(sigThr->tid), NULL, sigThreadWork, sigThr);
    if (err != 0) ERR("Couldn't create signal thread");
}

void InitializeNYC_NoMtx(const int N, int* arrNYC)
{
    yellow();
    printf("King Kong starting index/position = 0\n");   
    printf("Starting Array(NYC Buildings):\n");
    for(int i=0; i<N;i++)
    {
        arrNYC[i]=0;
        printf("%d ",arrNYC[i]);
    }
    reset();
    printf("\n"); //fflushes 
}

void printArrNYC(int N, int* arrNYC, pthread_mutex_t** arr_mtxArrayBeingRead) 
{
    for(int i=0; i<N;i++)
    {
        pthread_mutex_lock(arr_mtxArrayBeingRead[i]);
        printf("%d ",arrNYC[i]);
        pthread_mutex_unlock(arr_mtxArrayBeingRead[i]);
    }
    printf("\n");
}

void resetAndPrintArrNYC(int N, int* arrNYC, pthread_mutex_t** arr_mtxArrayBeingRead) 
{
    for(int i=0; i<N;i++)
    {
        pthread_mutex_lock(arr_mtxArrayBeingRead[i]);
        arrNYC[i]=0;
        printf("%d ",arrNYC[i]); //could've just done printf("0 0... 0") but didn't do it just for testing/fun.
        pthread_mutex_unlock(arr_mtxArrayBeingRead[i]);
    }
    printf("\n");
}

void createBuildingThreads(const int N, threadArgs* tArgs, int* arrNYC, pthread_mutex_t** arr_mtxArrayBeingRead)
{
    for(int i=0;i<N;i++)
    {
        tArgs[i].seed= rand();
        tArgs[i].toIncrement=&(arrNYC[i]);
        tArgs[i].mtxCurrArrPosBeingRead=arr_mtxArrayBeingRead[i];
        // tArgs[i].arr
    }

    for (int i = 0; i < N; i++) 
    {
        int err = pthread_create(&(tArgs[i].tid), NULL, threadWork, &tArgs[i]);
        if (err != 0) ERR("Couldn't create thread");
    }
}

void CancelAndJoinThreads(const int N, threadArgs* tArgs)
{
    for (int i = 0; i < N; i++) 
    {
        pthread_cancel(tArgs[i].tid);
    }
    for (int i = 0; i < N; i++) 
    {
        int err = pthread_join(tArgs[i].tid,NULL); //pthread cancel directly followed by join prevents trying to access data parent has freed already.
        if (err != 0){ errno = err; ERR("Can't join with a thread");}
    }

}

int KingKongWork(const int N, const int T, sigThreadArg* pSigThr, pthread_mutex_t** pMtxArrayBeingRead, int* arrNYC)
{
    int timeSecToContinueFrom=0;
    int kkIndex=0; //starting from 0.
    int kkFinalIndex=0; 
    pthread_mutex_lock(pSigThr->mtxSigIntRecv);
    do
    {
        if(*pSigThr->sigIntRecvd!=0)
        {
            *pSigThr->sigIntRecvd = 0;
            pthread_mutex_unlock(pSigThr->mtxSigIntRecv);
            red();
            printf("SIGINT RECVD=> arrNYC RESET only => ARRAY: \n");
            resetAndPrintArrNYC(N,arrNYC,pMtxArrayBeingRead);
            reset();
        }
        else
        {
            pthread_mutex_unlock(pSigThr->mtxSigIntRecv);
        }

        pthread_mutex_lock(pSigThr->mtxSigIntRecv);
        for(int timeSec=timeSecToContinueFrom;kkIndex<N-1&& timeSec<T && *pSigThr->sigIntRecvd==0; timeSec+=1) //N-1 is the last possible index, so we break out once we reach there.
        {
            pthread_mutex_unlock(pSigThr->mtxSigIntRecv);
            usleep(1 Sec2Mili Mili2Micro);
            //No chance of deadlocks because each mutex in array mtxArrayBeingRead is independant from each other (as in, they aren't dependant/connected (directly or indirectly) in any other thread except this one.)
            pthread_mutex_lock(pMtxArrayBeingRead[kkIndex]);
            pthread_mutex_lock(pMtxArrayBeingRead[kkIndex+1]);
            if(arrNYC[kkIndex]<arrNYC[kkIndex+1])
            {
                pthread_mutex_unlock(pMtxArrayBeingRead[kkIndex]);
                pthread_mutex_unlock(pMtxArrayBeingRead[kkIndex+1]);
                kkIndex+=1;
                printf("KKpos= %d ; Array: ",kkIndex);
                printArrNYC(N,arrNYC,pMtxArrayBeingRead);
            }
            else
            {
                pthread_mutex_unlock(pMtxArrayBeingRead[kkIndex]);
                pthread_mutex_unlock(pMtxArrayBeingRead[kkIndex+1]);
            }
            pthread_mutex_lock(pSigThr->mtxSigIntRecv);
        }
        pthread_mutex_unlock(pSigThr->mtxSigIntRecv);

        fprintf(stdout,"DONE\n");
        kkFinalIndex=kkIndex;
        pthread_mutex_lock(pSigThr->mtxSigIntRecv);
    } while (*pSigThr->sigIntRecvd!=0);
    pthread_mutex_unlock(pSigThr->mtxSigIntRecv);
    return kkFinalIndex;
}