#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#define NAME_LEN 80
struct save_header{
	void* start;
	void* end;
	char rwxp[4];
	char name[NAME_LEN];
	int data_size;
	int is_context;
};
int main(){
	int checkpoint_fd = open("myckpt",O_RDONLY);
	int res = -2;
	struct save_header header;
	while(read(checkpoint_fd,&header,sizeof(header)) == sizeof(header)){
		printf("this save header had data at %p - %p, the permissions were %.4s, the name of the file was %s, the size of the data is %d and the context boolean value is %d \n \n",
			       	header.start,header.end,header.rwxp,header.name,header.data_size,header.is_context);

		if(header.is_context == 0){
			char* data = malloc(header.data_size);
			res = read(checkpoint_fd,data,header.data_size);
			//figure out how to properly output data
			free(data);
		}
		if(header.is_context == 1){
			ucontext_t context;
			res = read(checkpoint_fd,&context,header.data_size);
			printf("this save header holds the context, the size of the context is %d \n its contents are %x \n",header.data_size,context);
			break;
		}
	}
		


}
