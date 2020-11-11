#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int max_arglen = sizeof(char)*32;   //max number of bytes for a single argument
    int i;  //index for args, total number of arguments - 1
    int j;  //index for current char in a single argument
    int k;  //pass args to pargs
    int len;
    int if_after_space; //mark if current char is after a space
    char args[MAXARG][max_arglen];   //restore all of arguments
    char buff;  //get user input char
    char* pargs[MAXARG];    //used to pass arguments to exec()
    //check input
    if(argc < 2){
        fprintf(2, "usage: xargs <cmd> ...\n");
        exit();
    }

    while(1){
        //clear args from the last input
        memset(args, 0, MAXARG * max_arglen);
        i = 0;
        if_after_space = 0;
        for(j=1; j<argc; j++){
            strcpy(args[i++], argv[j]);
        }

        j = 0;  //index for current char in a single argument
        //get a token from user input
        while(i < MAXARG -1){
            
            //CTRIl + D
            if((len = read(0, &buff, sizeof(char))) <= 0){
                wait();
                exit();
            }

            //'\n', finish reading this line
            if(buff == '\n'){
                break;
            }

            //space, finish reading an argument
            if(buff == ' '){
                if (if_after_space){
                    continue;
                }

                else{
                    if_after_space = 1;
                    i++;
                    j = 0;
                    continue;   //do not read space
                }
            }

            //read this char
            args[i][j] = buff;
            j++;
            if_after_space = 0;
        }

        for(k=0; k<MAXARG; k++){
            pargs[k] = args[k];
        }
        pargs[MAXARG-1] = 0;
        
        //execute command
        if(fork() == 0){
            exec(argv[1], pargs);
            exit();
        }

    }
    exit();
}