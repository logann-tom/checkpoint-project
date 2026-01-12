#include <sys/mman.h>
#include <unistd.h>
#include <ucontext.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define NAME_LEN 80
struct save_header{
        void* start;
        void* end;
        char rwxp[4];
        char name[NAME_LEN];
        int data_size;
        int is_context;
};

void do_work();
//Makes sure our stack for restart is far enough away from the memory addresses stored from checkpointing
//Will call it with recursive(1000)
void recursive(int levels){
	if(levels > 0){
		recursive(levels - 1);

	} else{
		//restore memory from the myckpt file
		do_work();
  }
}

void do_work(){
	int checkpoint_fd = open("myckpt",O_RDONLY);
	struct save_header cur_header;
	while(read(checkpoint_fd,&cur_header,sizeof(cur_header)) == sizeof(cur_header)){
		if(!cur_header.is_context){
			//get permissions for this section of memory
			int prot = PROT_WRITE;
			if(cur_header.rwxp[0] == 'r'){ prot |= PROT_READ;}
			//if(cur_header.rwxp[1] == 'w'){ prot |= PROT_WRITE;}
			if(cur_header.rwxp[2] == 'x'){ prot |= PROT_EXEC;}
			int map = MAP_FIXED | MAP_PRIVATE;
			
			//get the file fd for the mapped file
			int associated_file_fd = -1;
			//check if segment isnt anonymous
			if(strcmp(cur_header.name, "ANONYMOUS_SEGMENT") != 0 && strcmp(cur_header.name,"[heap]") != 0 && strcmp(cur_header.name,"[stack]") != 0){
				associated_file_fd = open(cur_header.name,O_RDONLY);
			} else{	
				map |= MAP_ANONYMOUS;
			}
			
			//Free the memory associated with this header
			void* addr = mmap(cur_header.start, cur_header.data_size, prot, map, associated_file_fd,0);
			//void* addr = mmap(cur_header.start,cur_header.data_size, prot,map ,associated_file_fd,0);
			if(addr == MAP_FAILED) {
			       	perror("mmap");
				printf("errno = %d\n", errno);	
				exit(1);
			}
			//we need to change reads internal offset as well otherwise read will try to read the data as a header
			int amt_read = 0;
			while(amt_read < cur_header.data_size){
				int temp = read(checkpoint_fd, (char*) addr + amt_read, cur_header.data_size - amt_read);
				amt_read += temp;
			}
	

		} else {
		//handle context
			ucontext_t old_registers;
			read(checkpoint_fd,&old_registers,sizeof(old_registers));
			printf("set context about to run.\n");
			setcontext(&old_registers);
		}
	}
	//use mmap(MAP_FIXED flag to map contents of the myckpt file back to memory.
}
int main(){
	recursive(1000);


}
