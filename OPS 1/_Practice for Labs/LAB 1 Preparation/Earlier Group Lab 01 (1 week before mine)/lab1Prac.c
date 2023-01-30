#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAXFD 20
#include <ftw.h>

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

typedef struct globalVars
{
    char* iArg;
    FILE* outputDest;
    char currArgLargestFileName[FILENAME_MAX];
    long currArgLargestFileSize;    //In bytes, so always int.
} globalVars;

globalVars globVar;


int checkOwn (struct stat filestat){

    if(filestat.st_uid==getuid())
        return 1;
    else
    {
        return -1;
    }
}

void usage(char* pname){
	fprintf(stderr,"USAGE:%s directories (at least one) -parameters(any, can be more than one\n",pname);
	exit(EXIT_FAILURE);
}

void red () {
  printf("\033[1;31m"); //only works with printf
}

void yellow () {
  printf("\033[1;33m"); //only works with printf
}

void reset () {
  printf("\033[0m");    //only works with printf
}

int walk(const char *name, const struct stat *s, int type, struct FTW *f)
{
    	struct stat filestat;
        if (lstat(name, &filestat)) ERR("lstat");
        int iOwn=checkOwn(filestat);
        if(iOwn==1)
        {
            switch (type){
                    case FTW_DNR:
                    break;
                    case FTW_D: 
/*
                        yellow();
                        printf("%s\n",name);
                        reset();
*/
                    break;
                    case FTW_F:
                        reset(); //just an empty statement to follow a label, as weirdly demanded by C
                        FILE* currFile;
                        int iArgMatches=0;
                        if((currFile=fopen(name,"r+"))==NULL)
                        {
                            //ERR("fgetc: ")    //commented because skip files that can't be opened instead of reporting error.
                            iArgMatches=1;
                        }
                        else
                        {
                            char currChar;
                            errno=-117;
                            if((currChar=fgetc(currFile))=='EOF')
                            {
                                if(errno==-117) //empty file
                                {
                                    iArgMatches=0;
                                }
                                else
                                {
                                    //ERR("fgetc: ")    //commented because skip files that can't be opened instead of reporting error, but that doesn't include truly empty files.
                                    iArgMatches=1;
                                }

                            }
                            else 
                            {
                                //THe following skips leading spaces.
    //                            if(currChar==' ')
    //                                while(getc(currFile)==' '){}
                                for(int i=0; i < strlen(globVar.iArg);i++)
                                {
                                    if(currChar==globVar.iArg[i])
                                    {
                                        iArgMatches=1;
                                    }
                                    else
                                    {
                                        iArgMatches=0;
                                        break;
                                    }
                                    errno=-117;
                                    if((currChar=fgetc(currFile))=='EOF'&&i+1<strlen(globVar.iArg))
                                    {
                                        if(globVar.iArg[i+1]!='\0')
                                        {
                                            if(errno==-117) //reached end normally.
                                            {
                                                iArgMatches=0;      //doesn't start with the string.
                                            }
                                            else //there was an error.
                                            {
                                                //ERR("fgetc: ")    //commented because skip files that can't be opened instead of reporting error.
                                                iArgMatches=1;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        

                        if(iArgMatches==0)
                        {
                            if(filestat.st_size>globVar.currArgLargestFileSize)
                            {
                                //memcpy(&(globVar.currArgLargestFileStat),&filestat,sizeof(struct stat));
                                globVar.currArgLargestFileSize=filestat.st_size;
                                strcpy(globVar.currArgLargestFileName,name);
                            }
                            red();
                            //printf("%s\n",name);
                            fprintf(globVar.outputDest,"%s\n",name);
                            reset();
                        }
                        break;
                    default : ;
            }
        }
        return 0;
}

int main(int argc, char** argv)
{
    //Argument handling:
    int opt; int gotI=0;
	while ((opt = getopt (argc, argv, "I:")) != -1)
    {  
        switch (opt)
        {
			case 'I':
                if(gotI)
                    usage(argv[0]);
                gotI=1;
                globVar.iArg=strdup(optarg);
				break;
			default: usage(argv[0]);
		}
    }
    //printf("%s\n",globVar.iArg);
	if(argc<2) usage(argv[0]);
//	for(i=0;i<argc-1;i++)
//		printf("Hello %s\n",argv[1]);
    char* L1_LOGFILE = getenv("L1_LOGFILE"); //use "export L1_LOGFILE=*some path*" on terminal to set it.
    int logfileExists=0;
    if(L1_LOGFILE!=NULL)
    {
        if((globVar.outputDest=fopen(L1_LOGFILE,"r+"))==NULL) //current;y r+ (read/write) but with the fopen equivalent to using open with only "O_RDWR" flag (no O_CREAT flags) //DEPRECATED COMMENT: Currently a [append(write only)] mode. Can be changed to w, w+, or a+ as needed.
        {
            if(errno==ENOENT)
            {
                fprintf(stdout,"The logfile path provided in env var L1_LOGFILE is invalid. Using stdout for output.\n");
                globVar.outputDest=stdout;
                //exit(EXIT_FAILURE);
            }
            else
            {
                ERR("fopen: ");
            }
        }
        else
        {
            logfileExists=1;
            //going to the end of file in order to append.
            if(fseek(globVar.outputDest, 0, SEEK_END)) ERR("fseek");
            int pos;
            if((pos=ftell(globVar.outputDest))==-1)
            {
                ERR("ftell(): ");
            }
            else if(pos==0)
            {
                fprintf(stdout,"The logfile provided in env var L1_LOGFILE is empty. Using stdout for output.\n");
                globVar.outputDest=stdout;
                //exit(EXIT_FAILURE);
            }
            fprintf(globVar.outputDest,"\n\n\n-----------------------PROGRAM BEGINS-----------------------\n");
        }
    }
    else
    {
        globVar.outputDest=stdout;
    }
    //going through directories:
    for(int i=1;i<=argc-1;i++)
    {
        if(strcmp(argv[i],"-I")==0)
        {
            i+=1;
            continue;
        }
        globVar.currArgLargestFileSize=-1;
        if(nftw(argv[i],walk,MAXFD,FTW_PHYS)!=0)
            ERR("nftw");
        if(globVar.currArgLargestFileSize==-1)
        {
            printf("For directory %s Largest File details are:\n ", argv[i]);
            printf("The directory contained no eligible files!\n");
        }
        else
        {
            printf("For directory %s Largest File details are:\n ", argv[i]);
            printf("|Size: %ld ||| Name: %s|\n", globVar.currArgLargestFileSize, globVar.currArgLargestFileName);
        }
    }
    //Closing out:
    free(globVar.iArg);
    if(logfileExists==1)
    {
        if(fclose(globVar.outputDest))ERR("fclose");
    }
    exit(EXIT_SUCCESS);   
}