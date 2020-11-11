#include "kernel/types.h"
#include "user/user.h"


int main(int argn, char *argv[]){
    //check cammand line input
	if(argn != 2){
		printf("require 2 arguments, but get %d: %s \n", argn, *argv);
		exit();
	}

	int sleepNum = atoi(argv[1]);
	printf("sleeping...\n");
	sleep(sleepNum);
	exit();
}