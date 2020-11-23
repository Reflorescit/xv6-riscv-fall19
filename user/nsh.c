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

void dispstr(char* s, char *e)
{
    while(s <= e){
        fprintf(2, "%c", *s);
        s++;
    }
    fprintf(2, "\n");
}



void runcmdunit(struct cmdunit* u)
{

    //redirection
    if(u->iredir.isvalid){
        close(u->iredir.fd);
        if(open(u->iredir.file, u->iredir.mode) < 0){
            fprintf(2, "open %s failed\n", u->iredir.file);
            exit(-1);
        }
    }
    if(u->oredir.isvalid){
        close(u->oredir.fd);
        if(open(u->oredir.file, u->oredir.mode) < 0){
            fprintf(2, "open %s failed\n", u->oredir.file);
            exit(-1);
        }
    }

    //exec
    if(u->argv[0] == 0)
        exit(-1);
    
    exec(u->argv[0], u->argv);
    fprintf(2, "exec %s failed\n", u->argv[0]);
    exit(-1);
}

void runcmd(struct cmd cmd)
/* execute cmd, and then exit 
    input: cmd to be executed
    output: none
*/
{
    int p[2];
    if (cmd.ispipe == 0){
        printf("no\n");
        runcmdunit(&cmd.left);
    }
        
    else{
        pipe(p);        
        if(myfork() == 0){
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            runcmdunit(&cmd.left);
        }
            
        if(myfork() == 0){
            close(0);
            dup(p[0]);
            close(p[0]);
            close(p[1]);
            runcmdunit(&cmd.right);
        }
        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
    }
    exit(0);
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
        // printf("argc: %d\n",argc);
        // dispstr(q, eq);
        if(argc >= MAXARGS){
            throwerror("too many args");
            }
        }
        else{
            throwerror("syntax error");
        }
    }
    cmdunit->argv[argc] = 0;
    cmdunit->eargv[argc] = 0;

    //Null-terminate
    int i;
    for (i = 0; i < argc; i++)
    {
        *cmdunit->eargv[i] = 0;
        //printf("%d: %s;%s\n ",i, cmdunit->argv[i], cmdunit->eargv[i]);
    }
    *cmdunit->iredir.efile = 0;
    *cmdunit->oredir.efile = 0;

}

struct cmd parsecmd(char *s)
{
    char *es, *p;  
    struct cmd cmd;
    es = s + strlen(s); //end of string
    cmd.ispipe = 0;    //default
    //dispstr(s,es);

    //not pipe
    if(!strchr(s, '|')){
        parseunit(&cmd.left, &s, es);
    }

    else{
        cmd.ispipe = 1;
        p = s;
        while(*p != '|')
            ++p;
        *p = 0;
        p++;
        //printf("p: %s\n",p);
        if(p < es && strchr(p,'|')){
            //printf("hey\n");
            throwerror("only support single |");
        }
        // printf("%s\n", s);
        // printf("%s\n", p);
        struct cmdunit u1;
        parseunit(&u1, &s, p-1);
        cmd.left = u1;
        struct cmdunit u2;
        parseunit(&u2, &p, es);
        cmd.right = u2;

    }

    return cmd;
}