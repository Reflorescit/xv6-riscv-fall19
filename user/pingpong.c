#include "kernel/types.h"
#include "user/user.h"

int main() {
    int parent[2];  //parent process writes, child process reads
    int child[2];   //child writes, parent reads

    pipe(parent);
    pipe(child);
    
    char* buffer = "";

    if(fork() == 0){
        if(read(parent[0], buffer, sizeof(buffer))){

            printf("%d: received ping\n", getpid());
            buffer = "pong";
            write(child[1], buffer, sizeof(buffer));

        }
        exit();
    }
    else{
        buffer = "ping";
        write(parent[1], buffer, sizeof(buffer));
        if(read(child[0], buffer, sizeof(buffer))){
            printf("%d: received pong\n", getpid());
        }
        wait();
        exit();
    }

}