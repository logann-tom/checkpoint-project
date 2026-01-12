#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char *argv[]){
	if(!getenv("LD_PRELOAD")){
		setenv("LD_PRELOAD", "/home/logantom/hw2-submission/libckpt.so",1);
		execvp(argv[0],argv);
	}
	sleep(1);
	int i = 0;
	while(1){
		printf("%d \n",i++);
		sleep(1);
	}
	return 0;
}
