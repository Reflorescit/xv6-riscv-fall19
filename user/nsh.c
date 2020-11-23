//OS lab2: my implementation of shell

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"


//represent command types
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

//max number of arguments allowed
#define MAXARGS 10
//max string length of input stream
#define MAXSTRLEN 100

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";


//general data structure of cmd, could not be passed to exec() directly
// struct cmd
// {
//     int cmd_type;

// }

struct redir
{
    int isvalid;
    char *file;
    char *efile;
    int mode;
    int fd; 
};


struct cmdunit
{
    int type;
    char *argv[MAXARGS];
    char *eargv[MAXARGS];
    struct redir iredir;
    struct redir oredir;
};

struct cmd
{
    int ispipe;
    struct cmdunit left;
    struct cmdunit right;
};



int myfork();   //fork with error message
void throwerror(char* s);  //display s and exit

struct cmd parsecmd(char* s);   //get executable command line from input stream

void runcmd(struct cmd cmd)
/* execute cmd, and then exit 
    input: cmd to be executed
    output: none
*/
{
    fprintf(2, "cmdtype: %d\n", cmd.left.type);
    fprintf(2, "iredir: %d\n", cmd.left.iredir.isvalid);
    fprintf(2, "oredir: %d\n", cmd.left.oredir.isvalid);
}

int readcmd(char* buf, int nbytes)
/* read n bytes from input istream, store in buf */ 
{
    memset(buf, 0, nbytes); //clear buf
    fprintf(2, "@ ");
    gets(buf, nbytes);
    return !(buf[0] == 0);
}

int main(void)
{
    static char buf[MAXSTRLEN];

     
    // from sh.c: Ensure that three file descriptors are open.
    int fd;
    while((fd = open("console", O_RDWR)) >= 0){
        if(fd >= 3){
            close(fd);
            break;
        }
    }

    //keep reading and executing input commands, until received CTRL+D
    while(readcmd(buf, sizeof(buf))){
        //fork and execute cmd
        if(myfork() == 0)
            runcmd(parsecmd(buf));
        wait(0);
    }
    exit(0);
}


void throwerror(char* s){
    fprintf(2, "%s\n", s);
    exit(-1);
}

int myfork(){
    int pid;
    if((pid = fork()) == -1){
        throwerror("fork failed");
    }
    return pid;
}

char gettoken(char **ps, char *es, char **q, char **eq)
/*
    modified from gettoken in sh.c, get one token from input string, uses *q and *eq to point at the 
    head and tail of it. The token might be a symbol or an argument
    output: if get a symbol return its name, or return 'a' for argument
*/
{
    char *s;
    char ret;
    s = *ps;
    //remove whitespace
    while(s < es && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    switch (*s)
    {
    case 0:
        ret = 0;
        break;
    //if get symbols
    case '|':
    case '<':
    case '>':
        ret = *s;
        s++;
        break;
    //if get token
    default:
        ret = 'a';
        //find the tail of the token
        while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
        s++;
        break;
    }
    if(eq)
        *eq = s;
    //remove whitespace
    while(s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return ret;
}

int ifcontain(char **ps, char *es, char *toks)
/* modified from peek in sh.c, return if the string pointed by *ps and es contains chars in toks */
{
  char *s;
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}



void parseunit(struct cmdunit* cmdunit, char **ps, char *es)
{
    int tok;
    int argc = 0;
    char *q, *eq;
    cmdunit->type = EXEC;
    cmdunit->iredir.isvalid = 0;
    cmdunit->oredir.isvalid = 0;
    // while(ifcontain(ps, es, "<>")){
    //     cmdunit->type = REDIR;
    //     tok = gettoken(ps, es, 0, 0);
    //     if(gettoken(ps, es, &q, &eq) != 'a')
    //         throwerror("missing file for redirection");
    //     switch(tok){
    //         case '<':
    //             cmdunit->file = q;
    //             cmdunit->efile = eq; 
    //             cmdunit->mode = O_RDONLY;
    //             cmdunit->fd = 0;
    //             break;
    //         case '>':
    //             cmdunit->file = q;
    //             cmdunit->efile = eq;
    //             cmdunit->mode = O_WRONLY|O_CREATE; 
    //             cmdunit->fd = 1;
    //             break;
    //     }
    // }
    // argc = 0;
    // while(!ifcontain(ps, es, "|")){
    //     if((tok=gettoken(ps, es, &q, &eq)) == 0)
    //         break;
    //     if(tok != 'a')
    //         throwerror("syntax");
    //     cmd->argv[argc] = q;
    //     cmd->eargv[argc] = eq;
    //     argc++;
    //     if(argc >= MAXARGS)
    //         throwerror("too many args");
    //     ret = parseredirs(ret, ps, es);
    // }
    // cmd->argv[argc] = 0;
    // cmd->eargv[argc] = 0;

    while(!ifcontain(ps, es, "|")){
        if((tok=gettoken(ps, es, &q, &eq)) == 0)
            break;
        
        if(tok == '<' || tok =='>'){
            if(gettoken(ps, es, &q, &eq) != 'a')
                throwerror("missing file for redirection");
            switch(tok){
            case '<':
                cmdunit->iredir.isvalid = 1;
                cmdunit->iredir.file = q;
                cmdunit->iredir.efile = eq; 
                cmdunit->iredir.mode = O_RDONLY;
                cmdunit->iredir.fd = 0;
                break;
            case '>':
                cmdunit->oredir.isvalid = 1;
                cmdunit->oredir.file = q;
                cmdunit->oredir.efile = eq;
                cmdunit->oredir.mode = O_WRONLY|O_CREATE; 
                cmdunit->oredir.fd = 1;
                break;
            }
            continue;
        }

        else if(tok == 'a'){
        cmdunit->argv[argc] = q;
        cmdunit->eargv[argc] = eq;
        argc++;
        if(argc >= MAXARGS)
            throwerror("too many args");
        }
        else{
            throwerror("syntax error");
        }
    }
    cmdunit->argv[argc] = 0;
    cmdunit->eargv[argc] = 0;

}

struct cmd parsecmd(char *s)
{
    char *es;  
    struct cmd cmd;
    es = s + strlen(s); //end of string
    cmd.ispipe = 0;    //default

    //not pipe
    if(!ifcontain(&s, es, "|")){
        cmd.left.type = EXEC;
        parseunit(&cmd.left, &s, es);
    }
    return cmd;

}