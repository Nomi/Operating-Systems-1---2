#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXPATH 500   //should try to find a predifined POSIX-based alternative (PATH_MAX? MAX_PATH?).
#define dotentryEXTENSIONLENGTH 6

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))


void usage(char* pname){
	fprintf(stderr,"USAGE:%s [-s|-g|-l] {-k lowercasestring (don't use with -l)} {-v string (only use with -s)}\n", pname);
	exit(EXIT_FAILURE);
}
#define USG(pname) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
            usage(pname))       //I created the USG macro to give find exactly which arguments were wrong (based on position of USG called)

// void set(char** argv,char* key,char* value,char* L1_DATABASE)
// {  
//     char path[MAXPATH];
//     sprintf(path,"./%s.entry",key);
//     if(L1_DATABASE!=NULL)
//     {
//         sprintf(path,"%s/%s.entry",L1_DATABASE,key);
//     }
//     FILE* s1;
//     s1=fopen(path,"w");
//     if(s1==NULL)
//     {
//         ERR("fopen");
//     }
//     fprintf(s1,"%s",value);
//     fclose(s1); //flushes automatically
// }

void set(char** argv,char* keyFile,char* value,char* parentDir)
{  
    char path[MAXPATH];
    sprintf(path,"%s%s",parentDir,keyFile);
    FILE* s1;
    s1=fopen(path,"w");
    if(s1==NULL)
    {
        ERR("fopen");
    }
    fprintf(s1,"%s",value);
    fclose(s1); //flushes automatically
}


void get(char** argv, char* keyFileName, char* parentDir)
{  
	// DIR *dirp;
	// struct dirent *dp;
    // char path[MAXPATH];
    // sprintf(path,"%s%s",parentDir,keyFile);
	//struct stat filestat;
	// if(NULL == (dirp = opendir(parentDir))) ERR("opendir");
    // int foundKey=0;
	// do {
	// 	if ((dp = readdir(dirp)) != NULL) 
    //     {
    //         //printf("%s,%s\n",dp->d_name,keyFileName);  // stdout is flushed by /n
    //         if(strcmp(dp->d_name,keyFileName)==0)
    //         {
    //             if((s1 = fopen(dp->d_name, "r")) != NULL) ERR("fopen");
    //             fprintf(stdout,"key:%s value:%s\n",)
    //             foundKey=1;
    //             break;
    //         }
	// 	}
	// } while (dp != NULL);
    // if(!foundKey)
    // {
    //     printf("Such a key couldn't be found in the database.\n");
    // }
	// if (errno != 0) ERR("readdir");
	//if(closedir(dirp)) ERR("closedir");
    char path[MAXPATH];
    sprintf(path,"%s%s",parentDir,keyFileName);
    char* key = strdup(keyFileName);
    key[(strlen(key)-1)-(dotentryEXTENSIONLENGTH-1)]='\0'; //marks end of string so that the extension doesn't print. //dotentryEXTENSION-1 because we want to change the . to '/0'
    FILE* s1;
    if((s1 = fopen(path, "r")) == NULL) ERR("fopen"); //should handle "file doesn't exist" as well!
    {
        if(fseek(s1, 0, SEEK_END)) ERR("fseek");
        long fsize = ftell(s1);
         if(fseek(s1, 0, SEEK_SET)) ERR("fseek");  /* same as rewind(f); */
        char *value = malloc(fsize + 1);
        fgets(value,fsize+1,s1);
        // //fgets stops reading upto newline ('\n'), but our task doesn't specify that value can have a '\n'/newline in it.
        // //Otherwise we just use the following (commented out) alternative:
        // fread(value, 1, fsize, s1); if(ferror(s1)){ERR()}; //ferror part detects error for fread. maybe should've used clearerr() before fread?
        printf("Key: %s , Value: %s\n", key,value);
        free(value);
    }
    free(key);
    if(fclose(s1)) ERR("fclose");
}

void list(char** argv, char* parentDir)
{
    DIR *dirp;
	struct dirent *dp;
	//struct stat filestat;
	if(NULL == (dirp = opendir(parentDir))) ERR("opendir");
	do {
		if ((dp = readdir(dirp)) != NULL) 
        {
            char validExtension[]={'.','e','n','t','r','y','\0'};
            int vExnLen=strlen(validExtension);
            int dnLen=strlen(dp->d_name);
            int isValidKeyFile=1;
            if(vExnLen<dnLen)
            {
                for(int i=dnLen-1,j=vExnLen-1;j>=0;i--,j--) //-1 becuz arrays are 0 indexed
                {
                    if(dp->d_name[i]!=validExtension[j])
                    {
                        isValidKeyFile=0;
                        break;
                    }
                }
                if(isValidKeyFile)
                {
                    get(argv,dp->d_name,parentDir);
                }
            }
		}
	} while (dp != NULL);
    printf("\n");
	if (errno != 0) ERR("readdir");
	if(closedir(dirp)) ERR("closedir");
}

int main(int argc, char** argv)
{
    //dealing with arguments
    char operation='a';
    int opt=-1;
    int indexOperation=-1;
    int indexKey=-1;
    int indexValue=-1;

    char*key=NULL;
    char*value=NULL;
    /* This and the following such commented block has been replaced by a single line from tutoria (it is right after the second block)
    int argcChecker=1;
    for(int i=1;i<argc;i++)
    {
        // printf("%s,%d\n",argv[i],strcmp(argv[i],"-sk"));
        // fflush(stdout);
        if(strcmp(argv[i],"-sk")==0||strcmp(argv[i],"-gk")==0)
        {
            argcChecker--;  //because argc considers -sk  to be one argument but getopt treats argcChecker increments for s and k seperately (similarly for others such as g)
        }
    }
    */
    if(argc<2)
        USG(argv[0]);
    else
    {
        while ((opt = getopt (argc, argv, "sglk:v:")) != -1)
        {  
            switch (opt)
            {
                case 's':
                    argcChecker++;
                    if(indexOperation!=-1)
                        USG(argv[0]);
                    operation='s';
                    indexOperation=optind;
                    break;
                case 'k':
                    argcChecker+=2;
                    if(indexKey!=-1)
                        USG(argv[0]);
                    indexKey=optind;
                    key=strdup(optarg);
                    for (int i = 0; i<(strlen(key)-1); i++)
                    {
                        if(isalpha(key[i])&&islower(key[i]))
                        {

                        }
                        else
                            USG(argv[0]);
                    }
                    break;
                case 'v':
                    argcChecker+=2;
                    if(indexKey==-1||indexValue!=-1)
                        USG(argv[0]);
                    value=strdup(optarg);
                    indexValue=optind;
                    break;
                case 'g':
                    if(indexOperation!=-1||indexValue!=-1)
                        USG(argv[0]);
                    operation='g';
                    indexOperation=optind;
                    argcChecker++;
                    break;
                case 'l':
                    if(indexOperation!=-1||indexKey!=-1||indexValue!=-1)
                        USG(argv[0]);
                    operation='l';
                    argcChecker++;
                    break;
                default: 
                    USG(argv[0]);
            }
        }
    }
    // printf("%d,%d",argc,argcChecker);
    // fflush(stdout);
    /* This and the previous such commented block has been replaced by a single line from tutorial (it's right after this block)
    if(argcChecker!=argc)
         USG(argv[0]);
    */
    if(argc>optind) USG(argv[0]);
    if(operation=='s'&&value==NULL)
        USG(argv[0]);
    if(operation=='g'&&key==NULL)
        USG(argv[0]);


    //Dealing with Enviornment Variables and Paths
    char* L1_DATABASE= getenv("L1_DATABASE");
    char keyFile[MAXPATH/3];
    char parentDir[-1+2*MAXPATH/3];
    //note that PATH will be ParentDir+KeyFile, so MaxPath had to be divided somehow. The current division was almost arbitrarily chosen by me.

    if(L1_DATABASE!=NULL)
    {
        if(indexKey!=-1)
            sprintf(keyFile,"%s.entry",key);
        if(L1_DATABASE[strlen(L1_DATABASE)-1]=='/') // \\==(normal \) because first \ is treated as escape character
            sprintf(parentDir,"%s",L1_DATABASE);
        else
            sprintf(parentDir,"%s/",L1_DATABASE);
    }
    else
    {
        sprintf(parentDir,"./");
        if(indexKey!=-1)
            sprintf(keyFile,"%s.entry",key);
    }

    //performing operations:
    switch(operation)
    {
        case 's':
            fprintf(stdout,"Writing %s to %s%s\n",value,parentDir,keyFile);
            //set(argv,key,value,L1_DATABASE);
            set(argv,keyFile,value,parentDir);
            break;
        case 'g':
            get(argv,keyFile,parentDir);
            break;
        case 'l':
            list(argv,parentDir);
            break;
    }


    //free memory:
    if(key!=NULL)
        free(key);
    if(value!=NULL)
        free(value);
}