#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <aio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#define BUFFERCOUNT_concurrent 3
#define BLOCKSIZE 128
#define SHIFT(counter, x) ((counter + x) % BUFFERCOUNT_concurrent)
void error(char *);
void usage(char *);
void siginthandler(int);
void sethandler(void (*)(int), int);
off_t getfilelength(int);
void fillaiostructs(struct aiocb *, char **, int, int);
void suspend(struct aiocb *);
void readdata(struct aiocb *, off_t);
void writedata(struct aiocb *, off_t);
void syncdata(struct aiocb *);
void getindexes(int *, int);
void cleanup(char **buffers, int* FDs, struct aiocb *aiolist);
void reversebuffer(char *, int);
void processblocks(struct aiocb *, char **, int*, int, int);
int suspendList(struct aiocb **aiolist,int listCount);
volatile sig_atomic_t work;
void error(char *msg){
        perror(msg);
        exit(EXIT_FAILURE);
}
void usage(char *progname){
        fprintf(stderr, "%s iFp iMFp oFp pl\n", progname);
        fprintf(stderr, "Arguments explanations: program_path input_file inpMask_file output_file parallellism_level\n");
        exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
        char *PiF,*PiMF,*PoF, *buffer[BUFFERCOUNT_concurrent];
        int iFD,imFD,oFD, p, blocksize, i;
        blocksize=BLOCKSIZE;
        struct aiocb aiocbs[BUFFERCOUNT_concurrent];
        if (argc != 5)
                usage(argv[0]);
        PiF = argv[1];
        PiMF = argv[2];
        PoF = argv[3];
        p=atoi(argv[4]);
        if (p<1)
                usage(argv[0]);
        work = 1;
        sethandler(siginthandler, SIGINT);
        if ((iFD = TEMP_FAILURE_RETRY(open(PiF, O_RDWR))) == -1)
                error("Cannot open inp file");
        if ((imFD = TEMP_FAILURE_RETRY(open(PiMF, O_RDWR))) == -1)
            error("Cannot open inp mask file");      
        if ((oFD = TEMP_FAILURE_RETRY(open(PoF, O_RDWR|O_TRUNC|O_CREAT))) == -1)
            error("Cannot open out file"); 
        int FDs[BUFFERCOUNT_concurrent]={iFD,imFD,oFD};
        int blockCounts[BUFFERCOUNT_concurrent];
        for(int i=0; i<BUFFERCOUNT_concurrent;i++)
        {
            blockCounts[i]=(getfilelength(FDs[i]) - 1)/blocksize;
        }
        if (blocksize > 0)
        {
            for (i = 0; i<BUFFERCOUNT_concurrent; i++)
                if ((buffer[i] = (char *) calloc (blocksize, sizeof(char))) == NULL)
                            error("Cannot allocate memory");
            fillaiostructs(aiocbs, buffer, 0, blocksize);
            for(int i=0; i<BUFFERCOUNT_concurrent;i++)
            {
                aiocbs[i].aio_fildes=FDs[i];
            }
            srand(time(NULL));
            processblocks(aiocbs, buffer, blockCounts, blocksize, 1);
            cleanup(buffer, FDs, aiocbs);
        }
        for(int i=0; i<BUFFERCOUNT_concurrent;i++)
        {
            if(TEMP_FAILURE_RETRY(close(FDs[i])) == -1)
                error("Cannot close file");
        }
        return EXIT_SUCCESS;
}




void siginthandler(int sig){
        work = 0;
}
void sethandler(void (*f)(int), int sig){
        struct sigaction sa;
        memset(&sa, 0x00, sizeof(struct sigaction));
        sa.sa_handler = f;
        if (sigaction(sig, &sa, NULL) == -1)
                error("Error setting signal handler");
}
off_t getfilelength(int fd){
        struct stat buf;
        if (fstat(fd, &buf) == -1)
                error("Cannot fstat file");
        return buf.st_size;
}
void suspend(struct aiocb *aiocbs){
        struct aiocb *aiolist[1];
        aiolist[0] = aiocbs;
        if (!work) return;
        while (aio_suspend((const struct aiocb *const *) aiolist, 1, NULL) == -1){
                if (!work) return;
                if (errno == EINTR) continue;
                error("Suspend error");
        }
        if (aio_error(aiocbs) != 0)
                error("Suspend error");
        if (aio_return(aiocbs) == -1)
                error("Return error");
}

int suspendList(struct aiocb **aiolist,int listCount){
        if (!work) return;
        while (aio_suspend((const struct aiocb *const *) aiolist, listCount, NULL) == -1){
                if (!work) return;
                if (errno == EINTR) continue;
                error("Suspend error");
        }
        int anyWasCompleted=0;
        for(int i=0;i<listCount;i++)
        {
            if (aio_error(aiolist[i]) != 0)
            {
                //This op didn't finish
            }
            else
            {
                if (aio_return(aiolist[i]) == -1)
                    error("Return error");
                anyWasCompleted=1;
                return i;
            }
        }
        if(anyWasCompleted==0)
        {
            error("Suspend error");
        }
        return -1;
}

void fillaiostructs(struct aiocb *aiocbs, char **buffer, int fd, int blocksize){
    int i;
    for (i = 0; i<BUFFERCOUNT_concurrent; i++){
            memset(&aiocbs[i], 0, sizeof(struct aiocb));
            aiocbs[i].aio_fildes = fd;
            aiocbs[i].aio_offset = 0;
            aiocbs[i].aio_nbytes = blocksize;
            aiocbs[i].aio_buf = (void *) buffer[i];
            aiocbs[i].aio_sigevent.sigev_notify = SIGEV_NONE;
    }
}
void readdata(struct aiocb *aiocbs, off_t offset){
        if (!work) return;
        aiocbs->aio_offset = offset;
        if (aio_read(aiocbs) == -1)
                error("Cannot read");
}
void writedata(struct aiocb *aiocbs, off_t offset){
        if (!work) return;
        aiocbs->aio_offset = offset;
        if (aio_write(aiocbs) == -1)
                error("Cannot write");
}
void syncdata(struct aiocb *aiocbs){
        if (!work) return;
        suspend(aiocbs);
        if (aio_fsync(O_SYNC, aiocbs) == -1)
                error("Cannot sync\n");
        suspend(aiocbs);
}
void getindexes(int *indexes, int max){
        indexes[0] = rand() % max;
        indexes[1] = rand() % (max - 1);
        if (indexes[1] >= indexes[0])
                indexes[1]++;
}
void cleanup(char **buffers, int* FDs, struct aiocb *aiolist){

    //The following is inefficient but works as a stopgap solution, which is fine for now.
    int aioReturn=-1;
    for(int i=0; i<BUFFERCOUNT_concurrent;i++)
    {
        int fd=FDs[i];
        for (;!work;)
        {
            if ((aioReturn=aio_cancel(fd, NULL)) == -1)
                error("Cannot cancel async. I/O operations");
            if(aioReturn==AIO_NOTCANCELED)
                while (aio_suspend((const struct aiocb *const *) aiolist, BUFFERCOUNT_concurrent, NULL) == -1){
                    if (errno == EINTR) continue;
                    error("Suspend error");
                }
            else
                break;
        }
    }

    for (int i = 0; i<BUFFERCOUNT_concurrent; i++)
            free(buffers[i]);
    for(int i=0; i<BUFFERCOUNT_concurrent;i++)
    {
        int fd=FDs[i];
        if (TEMP_FAILURE_RETRY(fsync(fd)) == -1)
                error("Error running fsync");
    }
}
void reversebuffer(char *buffer, int blocksize){
        int k;
        char tmp;
        for (k = 0; work && k < blocksize / 2; k++){
                tmp = buffer[k];
                buffer[k] = buffer[blocksize - k - 1];
                buffer[blocksize - k - 1] = tmp;
        }
}
void processblocks(struct aiocb *aiocbs, char **buffer, int* bcounts, int bsize, int iterations){
        int k;
        // fprintf(stdout,"%d , %d",bcounts[0],bcounts[1]);
        // getchar();
    for(k=0;k<bcounts[0]&&k<bcounts[1];k++)
    {
        //     printf("here\n");
            if(!work)
            {
                    break;
            }
        if(k>0)
        {
                writedata(&aiocbs[2],(k-1)*bsize);
        }
        readdata(&aiocbs[0], k*bsize);
        readdata(&aiocbs[1], k*bsize);
        int aios2wait4=2;
        // while(aios2wait4>0)
        // {
        //     int completedIndex;
        //         printf("aiocbs[1]\n");
        //     completedIndex=suspendList(&aiocbs,2);
        //     aios2wait4--;
        //     fprintf(stdout,"%s\n",(char*)aiocbs[completedIndex].aio_buf);
        // }
        int listCount=2;
        int readFlags[BUFFERCOUNT_concurrent];
        for(int i=0;i<BUFFERCOUNT_concurrent;i++)
        {
                readFlags[i]=0;
        }
        int anyWasCompleted=0;
        while(aios2wait4>0)
        {
                // printf("sub_here\n");
                const struct aiocb * aiolist[BUFFERCOUNT_concurrent];
                for(int i=0;i<BUFFERCOUNT_concurrent;i++)
                {
                        aiolist[i]=&aiocbs[i];
                }
                while (aio_suspend((const struct aiocb *const *) aiolist, listCount, NULL) == -1){
                        if (!work) return;
                        if (errno == EINTR) continue;
                        error("Suspend error");
                }
                for(int i=0;i<listCount;i++)
                {
                        // printf("Sub_sub_here (i=%i)\n",i);
                        if(readFlags[i]!=0)
                                continue;
                        int returnCount=0;
                        // printf("-\n",i);
                        if (aio_error(aiolist[i]) != 0)
                        {
                                //anyWasCompleted=0;
                        }
                        else
                        {
                                // printf("################################# (i=%i)\n",i);
                                if ((returnCount=aio_return(aiolist[i])) == -1)
                                        error("Return error");
                                anyWasCompleted=1;
                                readFlags[i]=1;
                                aios2wait4--;
                                fprintf(stdout,"------------ i=%d (Size Read=%d) ------------\n", i,returnCount);
                                fprintf(stdout,"%s\n",buffer[i]);
                        }
                }
                if(anyWasCompleted==0)
                {
                        error("Suspend error");
                        return -1;
                }
        }
        if(k>0)
        {
                fsync(aiocbs[2].aio_fildes);
                suspend(&aiocbs[2]);
        }
        for(int i=0; i<bsize;i++)
        {
                if(buffer[1][i]==' ')
                {
                        buffer[2][i]=buffer[0][i];
                }
                else
                {
                        buffer[2][i]=buffer[1][i];
                }
        }
    }
        writedata(&aiocbs[2],(k-1)*bsize);
        fsync(aiocbs[2].aio_fildes);
        suspend(&aiocbs[2]);
    // int curpos, j, index[2];
    // iterations--;
    // curpos = iterations == 0 ? 1 : 0;
    // readdata(&aiocbs[1], bsize * (rand() % bcount));
    // suspend(&aiocbs[1]);
    // for (j = 0; work && j<iterations; j++){
    //         getindexes(index, bcount);
    //         if (j > 0) writedata(&aiocbs[curpos], index[0] * bsize);
    //         if (j < iterations-1) readdata(&aiocbs[SHIFT(curpos, 2)], index[1] * bsize);
    //         reversebuffer(buffer[SHIFT(curpos, 1)], bsize);
    //         if (j > 0) syncdata(&aiocbs[curpos]);
    //         if (j < iterations-1) suspend(&aiocbs[SHIFT(curpos, 2)]);
    //         curpos = SHIFT(curpos, 1);
    // }
    // if (iterations == 0) reversebuffer(buffer[curpos], bsize);
    // writedata(&aiocbs[curpos], bsize * (rand() % bcount));
    // suspend(&aiocbs[curpos]);
}


