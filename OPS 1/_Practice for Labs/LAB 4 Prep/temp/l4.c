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
#define BLOCKSIZE 196
#define POSwrite(pos) (pos+BUFFERCOUNT_concurrent)
void error(char *);
void usage(char *);
void siginthandler(int);
void sethandler(void (*)(int), int);
off_t getfilelength(int);
void fillaiostructs(struct aiocb *, char **, int*, int);
void suspend(struct aiocb *);
void readdata(struct aiocb *, off_t);
void writedata(struct aiocb *, off_t);
void syncdata(struct aiocb *);
void getindexes(int *, int);
void cleanup(char **buffers, int* FDs, struct aiocb *aiolist);
void processblocks(struct aiocb *, char **, int*, int, int);
void buffersContentSort(char** buffers,int blocksize);
volatile sig_atomic_t work;
void error(char *msg){
        perror(msg);
        exit(EXIT_FAILURE);
}
void usage(char *progname){
        fprintf(stderr, "%s path_F1 path_F1 path_F1 parallellism_level\n",progname);
        exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
        char *path_F1,*path_F2,*path_F3, *buffer[2*BUFFERCOUNT_concurrent];
        int f1FD,f2FD,f3FD, p, i;
        struct aiocb aiocbs[2*BUFFERCOUNT_concurrent];
        if (argc != 5)
                usage(argv[0]);
        path_F1 = argv[1];
        path_F2 = argv[2];
        path_F3 = argv[3];
        p=atoi(argv[4]);
        if (p<1)
                usage(argv[0]);
         work = 1;
        // sethandler(siginthandler, SIGINT);
        if ((f1FD = TEMP_FAILURE_RETRY(open(path_F1, O_RDWR))) == -1)
                error("Cannot open inp file");
        if ((f2FD = TEMP_FAILURE_RETRY(open(path_F2, O_RDWR))) == -1)
            error("Cannot open inp mask file");      
        if ((f3FD = TEMP_FAILURE_RETRY(open(path_F3, O_RDWR))) == -1)
            error("Cannot open out file"); 
        int FDs[BUFFERCOUNT_concurrent]={f1FD,f2FD,f3FD};
        int blockCounts[BUFFERCOUNT_concurrent];
        for(int i=0; i<BUFFERCOUNT_concurrent;i++)
        {
            blockCounts[i]=(getfilelength(FDs[i]) - 1)/BLOCKSIZE; //-1 for newline char
                fprintf(stdout,"%d :here %d\n",blockCounts[i],__LINE__);
        }
        if (BLOCKSIZE > 0)
        {
            for (i = 0; i<2*BUFFERCOUNT_concurrent; i++)
                if ((buffer[i] = (char *) calloc (BLOCKSIZE, sizeof(char))) == NULL)
                            error("Cannot allocate memory");
            fillaiostructs(aiocbs, buffer, FDs, BLOCKSIZE);
            processblocks(aiocbs, buffer, blockCounts, BLOCKSIZE, 1);
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

void fillaiostructs(struct aiocb *aiocbs, char **buffer, int* fds, int blocksize){
    int i;
    for (i = 0; i<2*BUFFERCOUNT_concurrent; i++){
            memset(&aiocbs[i], 0, sizeof(struct aiocb));
            aiocbs[i].aio_fildes = fds[i%BUFFERCOUNT_concurrent];
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


void processblocks(struct aiocb *aiocbs, char **buffer, int* bcounts, int bsize, int iterations){
        int k;
//             for(k=0;k<1&&k<bcounts[0]&&k<bcounts[1]&&k<bcounts[2];k++)
        for(k=0;k<=bcounts[0]&&k<=bcounts[1]&&k<=bcounts[2];k++)
    {
            if(!work)
            {
                    break;
            }
        if(k>0)
        {
                writedata(&aiocbs[3], k*bsize);
                writedata(&aiocbs[4], k*bsize);
                writedata(&aiocbs[5], k*bsize);
        }
        readdata(&aiocbs[0], k*bsize);
        readdata(&aiocbs[1], k*bsize);
        readdata(&aiocbs[2],k*bsize);
        int aios2wait4=3;
        int listCount=3;
        int readFlags[BUFFERCOUNT_concurrent];
        for(int i=0;i<BUFFERCOUNT_concurrent;i++)
        {
                readFlags[i]=0;
        }
        while(aios2wait4>0)
        {
                int anyWasCompleted=0;
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
                        if(readFlags[i]!=0)
                                continue;
                        int returnCount=0;
                        if (aio_error(aiolist[i]) == 0)
                        {
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
                }
        }
        buffersContentSort(buffer,bsize);

        if(k>0)
        {
                suspend(&aiocbs[3]);
                suspend(&aiocbs[4]);
                suspend(&aiocbs[5]);
                if (aio_fsync(O_SYNC, &aiocbs[3]) == -1)
                        error("Cannot sync\n");
                if (aio_fsync(O_SYNC, &aiocbs[4]) == -1)
                        error("Cannot sync\n");
                if (aio_fsync(O_SYNC, &aiocbs[5]) == -1)
                        error("Cannot sync\n");
                suspend(&aiocbs[3]);
                suspend(&aiocbs[4]);
                suspend(&aiocbs[5]);
                printf("\n\n\n\n%s\n\n\n%s\n\n\n%s\n\n\n",buffer[3],buffer[4],buffer[5]);
        }
    }
        writedata(&aiocbs[3], k*bsize);
        writedata(&aiocbs[4], k*bsize);
        writedata(&aiocbs[5], k*bsize);
        suspend(&aiocbs[3]);
        suspend(&aiocbs[4]);
        suspend(&aiocbs[5]);
        if (aio_fsync(O_SYNC, &aiocbs[3]) == -1)
                error("Cannot sync\n");
        if (aio_fsync(O_SYNC, &aiocbs[4]) == -1)
                error("Cannot sync\n");
        if (aio_fsync(O_SYNC, &aiocbs[5]) == -1)
                error("Cannot sync\n");
        suspend(&aiocbs[3]);
        suspend(&aiocbs[4]);
        suspend(&aiocbs[5]);
        printf("\n\n\n\n%s\n\n\n%s\n\n\n%s\n\n\n",buffer[3],buffer[4],buffer[5]);
        // writedata(&aiocbs[2],(k-1)*bsize);
        // fsync(aiocbs[2].aio_fildes);
        suspend(&aiocbs[2]);
}



void buffersContentSort(char** buffers,int blocksize)
{
        for(int i=0;i<blocksize;i++)
        {
                char a, b,c;
                a=buffers[0][i];
                b=buffers[1][i];
                c=buffers[2][i];
                if(a>=b&&a>=c)
                {
                        if(b>=c)
                        {
                                buffers[3][i]=c;
                                buffers[4][i]=b;
                                buffers[5][i]=a;
                        }
                        else
                        {
                                buffers[3][i]=b;
                                buffers[4][i]=c;
                                buffers[5][i]=a;
                        }
                }
                else if(b>=a&&b>=c)
                {
                        if(a>=c)
                        {
                                buffers[3][i]=c;
                                buffers[4][i]=a;
                                buffers[5][i]=b;
                        }
                        else
                        {
                                buffers[3][i]=a;
                                buffers[4][i]=c;
                                buffers[5][i]=b; 
                        }
                }
                else if(c>=a&&c>=b)
                {
                        if(a>=c)
                        {
                                buffers[3][i]=b;
                                buffers[4][i]=a;
                                buffers[5][i]=c; 
                        }
                        else
                        {                                
                                buffers[3][i]=a;
                                buffers[4][i]=b;
                                buffers[5][i]=c; 
                        }
                }
        }
}