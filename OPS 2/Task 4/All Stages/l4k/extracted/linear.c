/*
 I declare that this piece of work which is the basis for recognition of achieving learning outcomes in the OPS course was completed on my own.
 Noman Noor 302343
*/
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

///Options based on define:
#define TILESEM                     //Switches to a variation of the program that solely relies on semaphores belonging to each tile during the game for synchronization. If left alone the program uses a single shared semaphore which locks before any player moves, using tiles semaphores only for randomization of starting positions.
#define NOBUSYWAITING               //Switches to the code modified to remove all busy-waiting scenarios.
#define SHOWBOARDATEND              //Shows board to the winner after winning.
//#define NONLOCAL                  //Switches from INADDR_LOOPBACK (localhost) to INADDR_ANY.


///Definitions:
#define BACKLOG 0 //for listen(....,backlog) backlog argument provides a hint to the  implementation  which  the implementation shall use to limit the number of outstanding connections in the socket's listen queue. If 0, it uses implementation defined value;
#define  PLAYER_DEAD -6969



///Macros:
#define ERR(source)                                                \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
   kill(0, SIGKILL), exit(EXIT_FAILURE))





///Global variables:
volatile sig_atomic_t last_signal=0 ;
//volatile int GAME=0;



///Struct definitions:


typedef struct {
    int* positions;
    //int* tiles;
    sem_t** tilesSem;
} boardStruct;

typedef struct {
    int playerNum;
    int cfd;
    pthread_barrier_t* barrier;
    int boardSize;
    int playerCount;
    boardStruct* board;
    sem_t* moveSem;
    //int** gameOngoing;
    sem_t* gameSem;
#ifdef NOBUSYWAITING
    pthread_t* tids;
#endif
} threadArguments;


///Function declarations (defined after main definition):
//--pre-existing functions--
int sethandler(void (*f)(int), int sigNo);
ssize_t bulk_read(int fd, char* buf, size_t count);
ssize_t bulk_write(int fd, char* buf, size_t count);
int bind_inet_socket(uint16_t port, int type);
int add_new_client(int sock_fd);
//--slightly modified from pre-existing functions--
void sig_handler(int sig);  //based on sigint_handler from tutorial 4 to allow more signals to be kept. I'm thinking about sigpipe in particular.
//--original functions' declarations from here on--//
void doServer(int fd, int playerCount, int boardSize);
void* worker(void* args);
int semLockedDeathByStepHandler(threadArguments* tArgs);
int deathByStepHandler(threadArguments* tArgs);
int haveWonYet(threadArguments* tArgs);
void printBoard_semLocked(threadArguments* tArgs,sem_t* lockedSem);
void printPositions_semLocked(threadArguments* tArgs,sem_t* lockedSem);
int zeroMsgHandler(threadArguments* tArgs);
int nonzeroMsgHandler(threadArguments* tArgs,int newPos,int i);
void* dummyWorker (void* barrier);

///Usage function definition:
void usage(char** argv) {
    fprintf(stderr, "USAGE: %s <port> <player_count> <board_size>.\n", argv[0]);
    exit(EXIT_FAILURE);
}


///Main function:
int main(int argc, char* argv[])
{
    if (sethandler(SIG_IGN, SIGPIPE))//makes sure if a client shuts down before receiving response, server doesn't exit (because pipe/connection wll be broken)
        ERR("Setting SIGPIPE handler:");
    if(sethandler(sig_handler,SIGINT))
        ERR("Setting SIGINT handler:");
    if(sethandler(sig_handler,SIGUSR1)) //I just need it to wake a thread whose player has been killed.
        ERR("Setting SIGUSR1 handler:");
    int port_num;
    int num_players;
    int board_size;
    if(argc!= 4)
        usage(argv);
    else
    {
        port_num=atoi(argv[1]);
        num_players=atoi(argv[2]);
        board_size=atoi(argv[3]);
        if(num_players>5||num_players<2)
        {
            usage(argv);
        }
        if(board_size<num_players||board_size>5*num_players)
            usage(argv);
    }

    int sock_fd = bind_inet_socket(port_num, SOCK_STREAM);
    //int flags = fcntl(sock_fd, F_GETFL) | O_NONBLOCK;
    //fcntl(sock_fd, F_SETFL, flags);
    doServer(sock_fd,num_players,board_size);
    if(TEMP_FAILURE_RETRY(close(sock_fd))<0)ERR("close");
    ///fprintf(stderr,"Server has terminated.\n");
    return EXIT_SUCCESS;
}






///Original Function definitions:

void doServer(int fd,int playerCount,int boardSize){
    //int* gameOngoing= (int*) malloc(sizeof(int*)); *gameOngoing=false;//false==0
    //int* tiles = (int*) malloc(boardSize*sizeof(int));
    //memset(tiles,0,boardSize*sizeof(int));//playerCount*sizeof(int));
    /* //memset test
    for(int i=0;i<boardSize;i++)
    {
        printf("%d -th:     %d\n",i,tiles[i]);

    }*/
    ///mallocs:
    sem_t** tilesSem;//= (sem_t**) malloc(boardSize*sizeof(sem_t*));
    /*for(int i=0; i<boardSize;i++)   //made me cry when it kept getting EAGAIN errnos using sem_trywait. God bless the guy who wrote this : https://stackoverflow.com/a/40556312 , even though it took avery long time to diagnose enough to find this. earlier I had sem* in boardStruct instead of sem** tilesSem and I just did tArgs->board->tilesSem=*tilesSem or something dumb like that. I wanna cry and I wanna die. I hate my life :''''((( WADINDSonsdfsakr03w4. //update: had to fix even more
    {
        tilesSem[i]=(sem_t*)malloc(sizeof(sem_t));
        if(sem_init(tilesSem[i], 0, 1)<0)
            ERR("sem_init: ");
    }*/
    int* positions;//= (int*) malloc(playerCount*sizeof(int));
    pthread_t* tids;//=(pthread_t*) malloc(playerCount*sizeof(pthread_t));
    threadArguments* tArgs;//=(threadArguments*) malloc((playerCount*sizeof(threadArguments)));
    ///mallocs end.
    sem_t gameSem;  int gameVal;
    if(sem_init(&gameSem, 0, playerCount)<0)
        ERR("sem_init: ");
    sem_t moveSem;
    if(sem_init(&moveSem, 0, 1)<0)
        ERR("sem_init: ");


    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, playerCount);

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    int playno=0;
    int cfd;
    int cfdStore[420];
    while(1){
        sem_getvalue(&gameSem,&gameVal);
        //fprintf(stdout,"gameVal semval = %d\n",gameVal);
        //fflush(stdout);
        if(playno<playerCount)
        {
            if(last_signal==SIGINT)
            {
                break;
            }
            if(select(fd+1,&rfds,NULL,NULL,NULL)>0){
                if((cfd=add_new_client(fd))>=0) {
                    char stringResponse[50];
                    cfdStore[playno] = cfd;
                    playno++;
                    sprintf(stringResponse, "you are player #%d. Please wait...\n", playno);
                    if (bulk_write(cfd, (char *) stringResponse, strlen(stringResponse)) < 0)
                        ERR("write:");
                    if (playno == 1) {
                        tilesSem = (sem_t **) malloc(boardSize * sizeof(sem_t *));
                        for (int i = 0; i <
                                        boardSize; i++)   //made me cry when it kept getting EAGAIN errnos using sem_trywait. God bless the guy who wrote this : https://stackoverflow.com/a/40556312 , even though it took avery long time to diagnose enough to find this. earlier I had sem* in boardStruct instead of sem** tilesSem and I just did tArgs->board->tilesSem=*tilesSem or something dumb like that. I wanna cry and I wanna die. I hate my life :''''((( WADINDSonsdfsakr03w4. //update: had to fix even more
                        {
                            tilesSem[i] = (sem_t *) malloc(sizeof(sem_t));
                            if (sem_init(tilesSem[i], 0, 1) < 0)
                                ERR("sem_init: ");
                        }
                        positions = (int *) malloc(playerCount * sizeof(int));
                        tids = (pthread_t *) malloc(playerCount * sizeof(pthread_t));
                        tArgs = (threadArguments *) malloc((playerCount * sizeof(threadArguments)));
                    }
                    if (errno != EPIPE) {
                        /*for(int i=0;i<playerCount;i++)
                        {
                            threadArguments tArgs = {.cfd=cfdStore[i],.playerNum=playno};*/

                        //pthread_t thread;
                        //pthread_t* tids=(pthread_t*) malloc
                        /*if (pthread_create(&thread, NULL, worker, (void*)&tArgs) != 0)///Handle sigpipe at the end for disconn
                            ERR("pthread_create");
                        if (pthread_detach(thread) != 0)
                            ERR("pthread_detach");*/
                        /*if (pthread_create(&tids[i], NULL, worker, (void*)&tArgs) != 0)///Handle sigpipe at the end for disconn
                            ERR("pthread_create");
                        if (pthread_detach(tids[i]) != 0)
                            ERR("pthread_detach");
                    }*/
                        //threadArguments tArgs = {.cfd=cfdStore[playno-1],.playerNum=playno,.barrier=&barrier};
                        tArgs[playno - 1].cfd = cfdStore[playno - 1];
                        tArgs[playno - 1].barrier = &barrier;
                        tArgs[playno - 1].playerNum = playno;
                        tArgs[playno - 1].playerCount = playerCount;
                        tArgs[playno - 1].boardSize = boardSize; /*tArgs[playno-1].board->tiles=&tiles;*/
                        tArgs[playno - 1].board = (boardStruct *) malloc(sizeof(boardStruct));
                        tArgs[playno - 1].board->positions = positions;
                        tArgs[playno -
                              1].board->tilesSem = tilesSem; //tArgs->gameOngoing=&gameOngoing;//(int*)malloc(sizeof(int*));tArgs->gameOngoing=gameOngoing;
                        tArgs[playno - 1].gameSem = &gameSem;
                        for (int i = 0; i < boardSize; i++) {
                            tArgs[playno - 1].board->tilesSem[i] = tilesSem[i];
                        }
                        tArgs[playno-1].board->tilesSem=tilesSem;
                        tArgs[playno - 1].moveSem = &moveSem;
#ifdef NOBUSYWAITING
                        tArgs[playno-1].tids=tids;
#endif
                        if (pthread_create(&tids[playno - 1], NULL, worker, (void *) &tArgs[playno - 1]) !=
                            0)///Handle sigpipe at the end for disconn
                            ERR("pthread_create");
                        if (pthread_detach(tids[playno - 1]) != 0)
                            ERR("pthread_detach");
                        //*gameOngoing=true; //true==1;
                        //GAME=true;
                    }
                }
                else
                    ERR("add_new_client: ");
            }
            else{
                if(errno==EINTR)
                {
                    if(playno!=0)   //Doing this to avoid changing my use of barriers, I guess you can say I'm doing it out of a weird mix of curiosity and laziness.
                    {
                        for(int i=0;i<2*(playerCount-playno);i++)   //multiplied by the barrier is encountered twice by the thread. THe leftover dummyWorkers will wait and help actual threads pass when the actual threads reach their second barrier (dummyWorker only has one).
                        {
                            pthread_t tid;
                            //fprintf(stdout,"creatingDummyThreads\n");
                            if(i>=(playerCount-playno))
                            {
                                usleep(20); //I've tested this as low as 10 ms, and this works still. I just kept it on the higher side for extra safety. Without usleep it doesn't work though, probably because of race conditon with the original worker thread in such a way that the more dummies than exactly-required amount manage to get in the same barrier making an actual worker stuck at the first barrier.
                                //fprintf(stdout, "latecomer is here.\n");
                            }
                            if (pthread_create(&tid, NULL, dummyWorker, (void *) &barrier) !=0)
                                ERR("pthread_create");
                            if (pthread_detach(tid) != 0)
                                ERR("pthread_detach");
                            //pthread_cancel(tid); alternative
                        }
                    }
                    break;
                }
                ERR("select");
            }
        }
        else if(gameVal==playerCount)//(!(*gameOngoing))//(!GAME)   //the inside of this if will only reach if the game is over, not when this is the first game because then playno<playernum and this else isn't reached
        {
            for(int i=0;i<playerCount;i++)
            {
                //free(tArgs->gameOngoing);
                //tArgs->gameOngoing=NULL;
                free(tArgs[i].board);
            }
            for(int i=0;i<boardSize;i++)
            {
                if(sem_destroy(tilesSem[i])<0)
                {
                    ERR("sem_destroy: ");
                }
            }
            free(tArgs);
            free(tids);
            free(tilesSem);
            free(positions);
            playno=0;
            //fprintf(stdout,"IT WORKS!!!!\n");
            if(last_signal==SIGINT)
            {
                //fprintf(stdout,"reaches here\n");
                break;
            }
        }
        else if(gameVal!=playerCount&&last_signal!=SIGINT)
        {
            /*if(last_signal==SIGINT)
            {
                int result= pthread_barrier_destroy(&barrier);
                if(result==EINVAL)
                {
                    continue;
                }
                else
                {
                    ERR("pthread_barrier_destroy: ");
                }
            }*/
            //struct timeval timval; timval.tv_sec=0; timval.tv_usec=100;
            if(select(fd+1,&rfds,NULL,NULL,NULL)<0)
            {
                if(errno!=EINTR)
                {
                    ERR("select: ");
                }
                else
                {
#ifdef NOBUSYWAITING
                    //fprintf(stdout,"sig:%d\n",last_signal);
                    for(int i=0;i<playerCount;i++)
                    {
                        if(tArgs[i].board->positions[i]>PLAYER_DEAD)
                        {
                            int killResult=pthread_kill(tArgs[i].tids[i],SIGUSR1);
                            if(killResult==EINVAL)
                            {
                                ERR("pthread_kill:");
                            }
                        }
                    }
#endif
                    continue;   //if I didn't continue after SIGINT then it'd go back to select after signal handling somehow.
                }
            }
            else
            {
                //questions:
                //is this gameVal fix okay //I have a semaphore which measures how many threads are still in the game loop/exist.
                //I use semaphores other than the tile protections to lock
                ///Edits to make:
                ///replace moveSem with tilesSem[newPos] and so on... if trylock fails, use sempost on it and kill the residing thread.
                sem_getvalue(&gameSem,&gameVal);
                if(gameVal!=playerCount&&last_signal!=SIGINT)
                {
                    int tempCFD;
                    if((tempCFD=add_new_client(fd))>=0) {
                        char stringResponse[99];

                        sprintf(stringResponse, "Permission denied: Game already in progress.\n");
                        if (bulk_write(tempCFD, (char *) stringResponse, strlen(stringResponse)) < 0)
                            ERR("write:");
                        if(TEMP_FAILURE_RETRY(close(tempCFD))<0)
                            ERR("close: ");
                    }
                    else
                        ERR("add_new_client: ");
                }
            }
        }
    }

    pthread_barrier_destroy(&barrier);

    if(sem_destroy(&moveSem)<0)
    {
        ERR("sem_destroy: ");
    }

    if(sem_destroy(&gameSem)<0)
    {
        ERR("sem_destroy: ");
    }

    fprintf(stdout,"Program terminated.\n");
    //free(gameOngoing);
    //free(tiles);
}

void* dummyWorker (void* barrier)
{
    pthread_barrier_t* barr=barrier;
    //fprintf(stdout,"dummyWorker waiting at barrier\n");
    pthread_barrier_wait(barr);
    return 0;
}

void* worker(void* args)    //break into sub functions taking argument threadArguments* tArgs and copy paste corresponding code.
{
    int previousPosition;
    threadArguments* tArgs=(threadArguments*)args;
    if(TEMP_FAILURE_RETRY(sem_trywait(tArgs->gameSem)<0))
    {
        ERR("sem_wait: ");   //for trywait: not handling EAGAIN because nothing in the task talks about handling simultaneous requests for the same player
    }
    char stringResponse[50];
    //fprintf(stdout,"waiting at barrier1\n");
    pthread_barrier_wait(tArgs->barrier);
    if(last_signal!=SIGINT)
    {
        sprintf(stringResponse, "The game has started.\n");
        if(bulk_write(tArgs->cfd,(char *)stringResponse,strlen(stringResponse))<0)
            ERR("write:");
    }
    srand(tArgs->playerNum);
    int currentPos=rand()%tArgs->boardSize;
    while(1)
    {
        int result= sem_trywait(tArgs->board->tilesSem[currentPos]);
        if(result!=0)
        {
            if(errno==EAGAIN)
            {
                //fprintf(stdout,"P%d,posattempt%d\n", tArgs->playerNum,currentPos);
                currentPos=rand()%tArgs->boardSize;
                //ERR("sem_trywait");       //I'll never remove (and only comment it out since it was for debugging purposes) this line because this is what helped me fix the issue mentioned above where I initialize the semaphores
                continue;
            }
            else
                ERR("sem_trywait: ");
        }
        else
        {
            //fprintf(stdout,"Lock acquired by P%d, result-%d \n",tArgs->playerNum,result);
            break;
        }
    }
    tArgs->board->positions[tArgs->playerNum-1]=currentPos;
    //sem_post(tArgs->board->tilesSem[currentPos]);
    //fprintf(stdout,"waiting at barrier2\n");
    pthread_barrier_wait(tArgs->barrier);   //waits for everyone to be done getting random position.
    //printPositions_semLocked(tArgs, tArgs->board->tilesSem[currentPos]);
    if(last_signal!=SIGINT)
    {
        printBoard_semLocked(tArgs, tArgs->board->tilesSem[currentPos]);
    }
#ifndef TILESEM
    if(TEMP_FAILURE_RETRY(sem_post(tArgs->board->tilesSem[currentPos]))<0)
        ERR("sem_post: ");
#endif
    while(last_signal!=SIGINT)//&&!flagAlreadyWonOrLost) //earlier 1.
    {
        int flagAlreadyWonOrLost=false;
        /*int16_tint message;*/
        /*int16_t*/ /* char* */ int acceptableMsgs[]={-2,-1,0,1,2};/*{"-2","-1","0","1","2"}*/;
        /*while(recv(tArgs->cfd,(char *)&message,sizeof(int16_t),0)<0){
            if(EINTR!=errno)ERR("recv:");
            if(SIGALRM==last_signal) break;
        }*/
        char message[50];
        int oldFlags = fcntl(tArgs->cfd, F_GETFL);
        int tempFlags = fcntl(tArgs->cfd, F_GETFL) | O_NONBLOCK;
        fcntl(tArgs->cfd, F_SETFL, tempFlags);
#ifndef NOBUSYWAITING
        int countRead=-10;
        while(countRead==-10)
        {
            if(last_signal==SIGINT)
            {
                break;
            }
            if(!flagAlreadyWonOrLost)
            {
                if(deathByStepHandler(tArgs)==-1)
                {
                    //if(TEMP_FAILURE_RETRY(sem_post(tArgs->gameSem))<0)
                    //    ERR("sem_post: ");
                    //return 0;
                    flagAlreadyWonOrLost=true;
                    break;
                }
            }
            countRead = read(tArgs->cfd, message, 40);
            if (countRead <= 0)
                countRead=-10;
            if(!flagAlreadyWonOrLost)
            {
                int temp= haveWonYet(tArgs);
                if(temp==1||temp==PLAYER_DEAD)
                {
                    //if(TEMP_FAILURE_RETRY(sem_post(tArgs->gameSem))<0)
                    //    ERR("sem_post: ");
                    //return 0;
                    flagAlreadyWonOrLost=true;
                    break;
                }
            }
        }
#else   //when NOBUSYWAITING IS DEFINED
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(tArgs->cfd, &rfds);
        if(select(tArgs->cfd + 1,&rfds,NULL,NULL,NULL)<0)
        {
            if(errno==EINTR)
            {
                char canExit[10]="";
                if(bulk_write(tArgs->cfd,(char *)canExit,strlen(canExit))<0)
                    ERR("write:");
                if(deathByStepHandler(tArgs)==-1)
                {
                    flagAlreadyWonOrLost=true;
                }
            }
            else
                ERR("select: ");
        }
        else
        {
            if(!flagAlreadyWonOrLost)
            {
                if(deathByStepHandler(tArgs)==-1)
                {
                    //if(TEMP_FAILURE_RETRY(sem_post(tArgs->gameSem))<0)
                    //    ERR("sem_post: ");
                    //return 0;
                    flagAlreadyWonOrLost=true;
                    break;
                }
            }
            if(last_signal==SIGINT)
            {
                break;
            }
            if(read(tArgs->cfd, message, 40)<=0)
                ERR("read");

            if(!flagAlreadyWonOrLost)
            {
                int temp= haveWonYet(tArgs);
                if(temp==1||temp==PLAYER_DEAD)
                {
                    //if(TEMP_FAILURE_RETRY(sem_post(tArgs->gameSem))<0)
                    //    ERR("sem_post: ");
                    //return 0;
                    flagAlreadyWonOrLost=true;
                    break;
                }
            }
        }
#endif
        if(last_signal==SIGINT||flagAlreadyWonOrLost)
        {
            break;
        }

    fcntl(tArgs->cfd,F_SETFL,oldFlags);


        for(int i=0;i<5&&last_signal!=SIGINT&&!flagAlreadyWonOrLost;i++)
        {

            //if(ntohs(message)==acceptableMsgs[i])
            if(atoi(message)==acceptableMsgs[i])//strcmp(message,acceptableMsgs[i])==0
            {

                fprintf(stdout,"%d\n",acceptableMsgs[i]);
                if(acceptableMsgs[i]==0)
                {
                    if(zeroMsgHandler(tArgs)==-1)
                    {
                        //if(TEMP_FAILURE_RETRY(sem_post(tArgs->gameSem))<0)
                        //    ERR("sem_post: ");
                        //return 0;
                        flagAlreadyWonOrLost=true;
                    }
                }
                else
                {
                    if(tArgs->board->positions[tArgs->playerNum-1]>PLAYER_DEAD) //might need to remove fix
                    {
                        if(TEMP_FAILURE_RETRY(sem_post(tArgs->board->tilesSem[currentPos]))<0)
                            ERR("sem_post: ");
                    }
                    previousPosition=currentPos;
                    int newPos = (currentPos+=acceptableMsgs[i]);

                    if(nonzeroMsgHandler(tArgs,newPos,i)==-1)
                    {
                        flagAlreadyWonOrLost=true;
                    }
                }
                break;
            }
            else
            {
//                if(TEMP_FAILURE_RETRY(sem_post(tArgs->board->tilesSem[currentPos]))<0)
//                    ERR("sem_post: ");
                continue;
            }
        }
        if(flagAlreadyWonOrLost||last_signal==SIGINT)
        {
            break;
        }
        if(deathByStepHandler(tArgs)==-1)
        {
            break;
        }
        if(haveWonYet(tArgs))
        {
            break;
        }
    }
    //if(TEMP_FAILURE_RETRY(close(tArgs->cfd))<0)
    //    ERR("close"); ///take to thread...
    //fprintf(stdout,"reaches here\n");
#ifndef TILESEM
    if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
        ERR("sem_post: ");
#elif defined(TILESEM)
    if(0<=tArgs->board->positions[tArgs->playerNum-1]&&tArgs->board->positions[tArgs->playerNum-1]<tArgs->boardSize)
    {
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->board->tilesSem[currentPos]))<0)
            ERR("sem_post: ");
    }
    else if(tArgs->board->positions[tArgs->playerNum-1]==PLAYER_DEAD)   //handles out of bounds. Otherwise sem_post is called in the thread of the player who stepped on the current player.
    {
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->board->tilesSem[previousPosition]))<0)
            ERR("sem_post: ");
    }
#endif
    if(TEMP_FAILURE_RETRY(sem_post(tArgs->gameSem))<0)
        ERR("sem_post: ");
    return 0;
}



int semLockedDeathByStepHandler(threadArguments* tArgs)
{
    if(tArgs->board->positions[tArgs->playerNum-1]<PLAYER_DEAD)
    {
#ifndef TILESEM
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
            ERR("sem_post: ");
#endif
        int killer=-1*(tArgs->board->positions[tArgs->playerNum-1]-PLAYER_DEAD); //simplifies to (-1*-killer's_tArgs->playerNum). i.e. = killer's playerNum
        char msgSteppedOnBy[420];
        sprintf(msgSteppedOnBy,"You lost: Player#%d stepped on you.\n", killer);
        if(bulk_write(tArgs->cfd,(char *)msgSteppedOnBy,strlen(msgSteppedOnBy))<0)
            ERR("write:");
        if(TEMP_FAILURE_RETRY(close(tArgs->cfd))<0)
            ERR("close: ");
        return -1;
    }
    else
    {
        return 0;
    }
}



int deathByStepHandler(threadArguments* tArgs)
{
#ifndef TILESEM
    while(TEMP_FAILURE_RETRY(sem_wait(tArgs->moveSem)<0))
    {
//                            if(errno==EAGAIN)
//                                continue;
//                            else
        ERR("sem_wait: ");   //for trywait: not handling EAGAIN because nothing in the task talks about handling simultaneous requests for the same player
    }
#endif
    if(tArgs->board->positions[tArgs->playerNum-1]<PLAYER_DEAD)
    {
#ifndef TILESEM
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
            ERR("sem_post: ");
#endif
        int killer=-1*(tArgs->board->positions[tArgs->playerNum-1]-PLAYER_DEAD); //simplifies to (-1*-killer's_tArgs->playerNum). i.e. = killer's playerNum
        char msgSteppedOnBy[420];
        sprintf(msgSteppedOnBy,"You lost: Player#%d stepped on you.\n", killer);
        if(bulk_write(tArgs->cfd,(char *)msgSteppedOnBy,strlen(msgSteppedOnBy))<0)
            ERR("write:");
        if(TEMP_FAILURE_RETRY(close(tArgs->cfd))<0)
            ERR("close: ");
        return -1;
    }
#ifndef TILESEM
    if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
        ERR("sem_post: ");
#endif
    return 0;
}


int haveWonYet(threadArguments* tArgs)
{
#ifndef TILESEM
    while(TEMP_FAILURE_RETRY(sem_wait(tArgs->moveSem)<0))
    {
        ERR("sem_wait: ");
    }
#endif
    if(tArgs->board->positions[tArgs->playerNum-1]==PLAYER_DEAD)
    {
        return PLAYER_DEAD;
    }
    if(semLockedDeathByStepHandler(tArgs)==-1)
    {
        return PLAYER_DEAD;
    }
    int playersAlive=0;
    for(int i=0;i<tArgs->playerCount;i++)
    {
        int temp=tArgs->board->positions[i];
        temp=temp;
        if(tArgs->board->positions[i]>PLAYER_DEAD)
        {
            playersAlive++;
        }
    }
    if(playersAlive==1)
    {
        char msgWin[420];
        sprintf(msgWin,"You have won!\n");
        if(bulk_write(tArgs->cfd,(char *)msgWin,strlen(msgWin))<0)
            ERR("write:");
#ifdef SHOWBOARDATEND
        printBoard_semLocked(tArgs,tArgs->moveSem);
#endif
        //*(tArgs->ptrGameOnGoing)=false;
        //GAME=false;
        //**(tArgs->gameOngoing)=false;
#ifndef TILESEM
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
            ERR("sem_post: ");
#endif
        if(TEMP_FAILURE_RETRY(close(tArgs->cfd))<0)
            ERR("close: ");
        return 1;
    }
    if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
        ERR("sem_post: ");
    return 0;
}

void printBoard_semLocked(threadArguments* tArgs,sem_t* lockedSem)
{
    //int currentPos=tArgs->board->positions[tArgs->playerNum-1];
    char positionStatusMessage[999];
    sprintf(positionStatusMessage,"Board: |");
    for(int i=0;i<tArgs->boardSize;i++)
    {
        char fill[40];
        sprintf(fill," ");
        for(int j=0; j<tArgs->playerCount;j++)
        {
            if(i==tArgs->board->positions[j])
            {
                int temp=j+1;
                sprintf(fill,"%d",temp);
                break;
            }
        }
        sprintf(positionStatusMessage+strlen(positionStatusMessage),"%s|",fill);
    }
    //if(sem_post(lockedSem)<0)
    //    ERR("sem_post: ");
    sprintf(positionStatusMessage+strlen(positionStatusMessage),"\n");
    if(bulk_write(tArgs->cfd,(char *)positionStatusMessage,strlen(positionStatusMessage))<0)
        ERR("write:");
}

void printPositions_semLocked(threadArguments* tArgs,sem_t* lockedSem)
{
    //int currentPos=tArgs->board->positions[tArgs->playerNum-1];
    char positionStatusMessage[420];
    sprintf(positionStatusMessage,"Player positions: ");
    for(int i=0;i<tArgs->playerCount;i++)
    {
        sprintf(positionStatusMessage+strlen(positionStatusMessage),"P%d - %d, ",i+1,tArgs->board->positions[i]);
    }
    //if(sem_post(lockedSem)<0)
    //    ERR("sem_post: ");
    sprintf(positionStatusMessage+strlen(positionStatusMessage),".\n");
    if(bulk_write(tArgs->cfd,(char *)positionStatusMessage,strlen(positionStatusMessage))<0)
        ERR("write:");
}


int zeroMsgHandler(threadArguments* tArgs)
{
#ifndef TILESEM
    while(TEMP_FAILURE_RETRY(sem_wait(tArgs->moveSem)<0))
    {
//                        if(errno==EAGAIN)
//                        {
//                            deathByStepHandler(tArgs);
//                            continue;
//                        }
//                        else
        ERR("sem_wait: ");
    }
#endif
    if(semLockedDeathByStepHandler(tArgs)==-1)
    {
        return -1;
    }
    printBoard_semLocked(tArgs,tArgs->moveSem);
#ifndef TILESEM
    if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
        ERR("sem_post: ");
#endif
    return 0;
}


#ifndef TILESEM
int nonzeroMsgHandler(threadArguments* tArgs, int newPos, int i)
{
    if(newPos<0||newPos>=tArgs->boardSize)
    {
        char msgOutOfBounds[420];
        sprintf(msgOutOfBounds,"You lost: You stepped out of the board.\n");
        while(TEMP_FAILURE_RETRY(sem_wait(tArgs->moveSem)<0))
        {
//                            if(errno==EAGAIN)
//                                continue;
//                            else
            ERR("sem_wait: ");   //now it's wait instead of trywait so EAGAIN won't even come. //not handling EAGAIN because nothing in the task talks about handling simultaneous requests for the same player
        }
        if(semLockedDeathByStepHandler(tArgs)==-1)
        {
            return -1;
        }
        tArgs->board->positions[tArgs->playerNum-1]=PLAYER_DEAD - 0;
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
            ERR("sem_post: ");
        if(bulk_write(tArgs->cfd,(char *)msgOutOfBounds,strlen(msgOutOfBounds))<0)
            ERR("write:");
        if(TEMP_FAILURE_RETRY(close(tArgs->cfd))<0)
            ERR("close: ");
        //pthread_exit(0);
        //exit(0);
        return -1;
    }
    else
    {   //since player can't move to the position they are already at, we don't need to synchronize using semaphores,etc. I.e. we don't need to consider the case that someone who is at a position is moving away at the same time.
        //while(sem_trywait(tArgs->board->tilesSem[currentPos])<0)
        while(TEMP_FAILURE_RETRY(sem_wait(tArgs->moveSem)<0))
        {
//                            if(errno==EAGAIN)
//                                continue;
//                            else
            ERR("sem_wait: ");   //for trywait: not handling EAGAIN because nothing in the task talks about handling simultaneous requests for the same player
        }
        if(semLockedDeathByStepHandler(tArgs)==-1)
        {
            return -1;
        }
        tArgs->board->positions[tArgs->playerNum-1]=newPos;
        for(int i=0;i<tArgs->playerCount;i++)
        {
            if(i!=(tArgs->playerNum-1)&&tArgs->board->positions[i]==newPos)
            {
                //fprintf(stdout,"%d kills %d,pc:%d\n",tArgs->playerNum,i+1,tArgs->playerCount);
                tArgs->board->positions[i]=PLAYER_DEAD-(tArgs->playerNum);
            }
        }
        //if(sem_post(tArgs->board->tilesSem[currentPos])<0)
        if(TEMP_FAILURE_RETRY(sem_post(tArgs->moveSem))<0)
            ERR("sem_post: ");
        return 0;
    }
}
#elif defined(TILESEM)
int nonzeroMsgHandler(threadArguments* tArgs, int newPos, int i)        //remember that sem_post is already called on the previous position when recieving a non-0 acceptable message
{
    if(newPos<0||newPos>=tArgs->boardSize)
    {

        char msgOutOfBounds[420];
        sprintf(msgOutOfBounds,"You lost: You stepped out of the board.\n");
        if(tArgs->board->positions[tArgs->playerNum-1]<PLAYER_DEAD)
        {
            return -1;
        }
        if(semLockedDeathByStepHandler(tArgs)==-1)
        {
            return -1;
        }
        tArgs->board->positions[tArgs->playerNum-1]=PLAYER_DEAD - 0;
#ifdef NOBUSYWAITING
        int* playersDead =(int*) malloc(sizeof(int));
        sem_getvalue(tArgs->gameSem,playersDead);
        (*playersDead)++; //because this thread hasn't sem_post-ed gameSem yet;
        if(tArgs->playerCount-(*playersDead)==1)
        {
            int winnerIndex=tArgs->playerNum-1;
            for(int i=0;i<tArgs->playerCount;i++)
            {
                if(i!=winnerIndex&&tArgs->board->positions[i]>=0)
                {
                    winnerIndex=i;
                    break;
                }
            }
            int killResult=pthread_kill(tArgs->tids[winnerIndex],SIGUSR1);
            if(killResult==EINVAL)
            {
                ERR("pthread_kill:");
            }
            free(playersDead);
        }
        else if(tArgs->playerCount-*playersDead==0)  //in case the winner steps out of bounds before it is declared the winner, this elseif ignores that.
        {
            free(playersDead);
            return 0;
        }
#endif
        if(bulk_write(tArgs->cfd,(char *)msgOutOfBounds,strlen(msgOutOfBounds))<0)
            ERR("write:");
        if(TEMP_FAILURE_RETRY(close(tArgs->cfd))<0)
            ERR("close: ");
        //pthread_exit(0);
        //exit(0);
        return -1;
    }
    else

    {
        if(semLockedDeathByStepHandler(tArgs)==-1)
        {
            return -1;
        }
        int a=false;
        while(sem_trywait(tArgs->board->tilesSem[newPos])<0)
        {
            if(errno==EAGAIN)
            {
                if(a)
                {
                    ERR("sem_trywait loop 2nd time:");
                }
                else
                {
                    a=true;
                }
                //fprintf(stdout,"p%d, reaches here!\n",tArgs->playerNum);
                for(int i=0;i<tArgs->playerCount;i++)
                {
                    if(newPos==tArgs->board->positions[i])
                    {
                        if(i!=(tArgs->playerNum-1)&&tArgs->board->positions[i]==newPos)
                        {
                            //fprintf(stdout,"%d kills %d,pc:%d\n",tArgs->playerNum,i+1,tArgs->playerCount);
                            if(TEMP_FAILURE_RETRY(sem_post(tArgs->board->tilesSem[newPos]))<0)
                                ERR("sem_post: ");
                            tArgs->board->positions[tArgs->playerNum-1]=newPos;
                            tArgs->board->positions[i]=PLAYER_DEAD-(tArgs->playerNum);
#ifdef NOBUSYWAITING
                            int killResult=pthread_kill(tArgs->tids[i],SIGUSR1);
                            if(killResult==EINVAL)
                            {
                                ERR("pthread_kill:");
                            }
#endif
                            break;
                        }
                    }
                }
                continue;
            }
            else
                ERR("sem_wait: ");
        }
        tArgs->board->positions[tArgs->playerNum-1]=newPos;
        return 0;
    }
}
#endif



















///Pre-existing functions:
int sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if(-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}
void sig_handler(int sig) {
    if(sig==SIGINT)
    {
        fprintf(stdout," - SIGINT received.\n");
    }
    if(last_signal!=SIGINT&&sig!=SIGUSR1) //gives SIGINT the highest priority;  //ignores setting SIGUSR1 because I'm already using it.
    {
        last_signal=sig;
    }
}

ssize_t bulk_write(int fd, char* buf, size_t count) {
    int c;
    size_t len = 0;
    do {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_read(int fd, char* buf, size_t count) {
    int c;
    size_t len = 0;

    do {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

int bind_inet_socket(uint16_t port, int type) {
    int sock_fd = socket(PF_INET, type, 0);
    if (sock_fd < 0)
        ERR("socket");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifdef NONLOCAL
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
#else
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    int t = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");

    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        ERR("bind");

    if (SOCK_STREAM == type)
        if (listen(sock_fd, BACKLOG) < 0)
            ERR("listen");

    return sock_fd;
}

int add_new_client(int sock_fd) {
    int client_sock_fd = TEMP_FAILURE_RETRY(accept(sock_fd, NULL, NULL));
    if (client_sock_fd < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
        ERR("accept");
    }

    return client_sock_fd;
}

