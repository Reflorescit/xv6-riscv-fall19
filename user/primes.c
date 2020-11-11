#include<kernel/types.h>
#include<user/user.h>

void Primes(int *numbers, int len){
    int p = numbers[0];
    int fd[2];
    int new_numbers[35];   //passed to child process
    int new_len = 0;

    int i;

    printf("prime %d\n", p);

    if (len == 1){
        exit();
    }
    else{
        pipe(fd);
        //parent process
        if (fork() != 0){
            //select all numbers which could not be divided by p
            for(i=1; i<len; i++){
                if(numbers[i] % p != 0){
                    *(new_numbers + new_len) = numbers[i];
                    new_len++;
                    //printf("%d\n", new_numbers[new_len-1]);
                }
            }

            //close(fd[0]);    //close readable end
            write(fd[1], new_numbers, new_len*sizeof(int));
            close(fd[1]);
            wait();
            exit();
            
        }
        else{
            close(fd[1]);
            while(read(fd[0], new_numbers+new_len, sizeof(int)) > 0){
                //printf("receiced %d\n", new_numbers[new_len]);
                new_len++;
            }
            Primes(new_numbers, new_len);
            exit();
        }
    }
}


int main(){
    int numbers[34];
    int i;
    for(i=0; i <34; i++){
        numbers[i] = i + 2;
    }

    Primes(numbers, 34);
    return 0;
}