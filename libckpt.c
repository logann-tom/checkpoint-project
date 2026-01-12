#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <asm/prctl.h>        /* Definition of ARCH_* constants */
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <errno.h> //having issues with write
static int is_in_signal_handler = 0;
static int is_restart;
ucontext_t context;
#define NAME_LEN 80
struct proc_maps_line {
	void *start;
	void *end;
	char rwxp[4];
	//int read write exectue;
	char name[NAME_LEN];
};
struct save_header {
	void *start;
	void *end;
	char rwxp[4];
	char name[NAME_LEN];
	int data_size;
	int is_context;
};

int fill_proc_maps_line(int proc_maps_fd, struct proc_maps_line *proc_maps_line, char *filename){
	unsigned long int start, end;
	char rwxp [4];
	char tmp [10];
	int tmp_stdin = dup(0); //dupe standard input because scanf reads from stdin so we want to put our proc map fd into the stdin
	dup2(proc_maps_fd,0); //copy our proc maps into standard input(0)
	int args_matched = scanf("%lx-%lx %4c %*s %*s %*[0-9 ]%[^\n]\n",
			&start, &end, rwxp, filename); //match regex and fill fields based on our matched regex. args_matched stores how many fields were matched

	dup2(tmp_stdin,0);//restore standard input to our copied standard input from befire
	close(tmp_stdin);
	//args_matched will give us these values when we are done with the file.
	if(args_matched == EOF || args_matched == 0){
		proc_maps_line -> start = NULL;
		proc_maps_line -> end = NULL;
		return EOF;
	} else if(args_matched == 3){//missing name
		strncpy(proc_maps_line -> name, "ANONYMOUS_SEGMENT", strlen("ANONYMOUS_SEGMENT") + 1);//copy our new string ("anonymous segment" into the old string in the struct
	} else{
		assert(args_matched == 4);
		//you cant directly assign a "string"(character array) in c the same way as you do in java so you have to copy it with strncpy otherwise you get errors
		strncpy(proc_maps_line -> name, filename, NAME_LEN-1);
		proc_maps_line->name[NAME_LEN-1] = '\0';

	}
	proc_maps_line -> start = (void *)start; //void* = generic pointer it points to memory location without knowing type in that memory
	proc_maps_line -> end = (void *)end;
	memcpy(proc_maps_line->rwxp, rwxp, 4); //same concept as strncpy but because we know rxwp is fixed length its better
	return 0;//success


}
int proc_self_maps(struct proc_maps_line proc_maps[]){
	//proc maps fd is a integer file descriptor that helps the os keep track of said file. we can pass it to locate the file again
	int proc_maps_fd = open("/proc/self/maps", O_RDONLY);
	char filename [100];
	int line_status = -4;
	int i = 0;
	for(i = 0; line_status != EOF; i++){
		line_status = fill_proc_maps_line(proc_maps_fd, &proc_maps[i], filename);
		//print each successful fill of proc_maps[]
		if(line_status == 0){
		//debugging	printf("proc_self_maps: filename %s\n",filename);
		}
	}
	close(proc_maps_fd);
	return 0;
}
void signal_handler(int signal){
	is_in_signal_handler = 1;
	//get context
	unsigned long saved_fs;
	syscall(SYS_arch_prctl, ARCH_GET_FS, &saved_fs);
	getcontext(&context);
	syscall(SYS_arch_prctl, ARCH_SET_FS, saved_fs);
	//set context will bring us exactly right back to where we called get context. this is_restart is basically telling the program we want to restart the program instead of doing the saving again.
	if(is_restart == 1){
		is_restart = 0;
		printf("RESTARTING\n");
		return;
	}
	else{
		is_restart = 1;
		//proc maps is array we will use to hold proc maps data. will be filled from proc_self_maps
		struct proc_maps_line proc_maps[1000];
		//check if this ran till end
		assert( proc_self_maps(proc_maps) == 0);
		int save_file_fd = open("myckpt",O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);//if file doesnt exist O_CREAT flag tells open to create it
		//check if proc maps saved and store the headers of the data and the proc maps data
		int i = 0;
		for (i = 0; proc_maps[i].start != NULL; i++) {
			struct proc_maps_line cur_line = proc_maps[i];
    			//debugging printf("%s (%c%c%c%c)\n"
           		//"  Address-range: %p - %p\n",
          		//	cur_line.name,
           		//	cur_line.rwxp[0], cur_line.rwxp[1], cur_line.rwxp[2],cur_line.rwxp[3],
           		//	cur_line.start, cur_line.end);
			char temprwxp[5];//cant compare rwxp to ---p because strcmp needs \0 terminated strings
			memcpy(temprwxp,cur_line.rwxp,4);
			temprwxp[4] = '\0';
			//we dont want to copy these files, guard files leads to infinite loop when trying to read it and the [v...] files break the read program
			if( strcmp(cur_line.name, "[vdso]") == 0 || strcmp(cur_line.name, "[vsyscall]") == 0 || strcmp(cur_line.name, "[vvar]") == 0 ||
					strcmp(temprwxp,"---p") == 0){
			//	printf("\n skipped entry \n");
				continue;
			}
			//write each proc map line header to a file	
			//use char* because when doing pointer math the difference is calculated in chunks of sizeof(type) and char is one byte. eg (int*) 20 - (int*) 0 = 5 but (char*) 20 - (char*) 0 = 20 
			size_t data_size = (char *) cur_line.end - (char *) cur_line.start; //cant do math on void* because the output of this memory address math is difference/sizeof(type)
			struct save_header proc_header; 
				proc_header.start = cur_line.start; //we use . here instead of -> like before because we have access to the struct when before we only had a pointer to the proc_maps_line
				proc_header.end = cur_line.end;
				proc_header.data_size = data_size;
				proc_header.is_context = 0;
				strncpy(proc_header.name, cur_line.name,NAME_LEN - 1);
				proc_header.name[NAME_LEN - 1] = '\0';
				memcpy(proc_header.rwxp, cur_line.rwxp,4); 
	       		int amt_written = 0;
			//look out for [vsdo] [vsyscall] and maybe [vvar] because these we will use the new one but for now try saving
			//use while because write may not write it all in the first go so basically just assuring that write finishes the job
			while(amt_written < sizeof(proc_header)){
				int temp = write(save_file_fd,(char*)&proc_header + amt_written ,sizeof(proc_header) - amt_written);
				amt_written += temp;
			}
			//write the data in that proc map line next
			amt_written = 0;
			while(amt_written < data_size){
				int temp = write(save_file_fd,(char*)cur_line.start + amt_written,data_size - amt_written);
				amt_written += temp;
			}
		}
		//write header to context
		struct save_header context_header;
		context_header.data_size = sizeof(context);
		context_header.is_context = 1;
		int amt_written = 0;
		while(amt_written < sizeof(context_header)){
			int temp = write(save_file_fd,(char*)&context_header + amt_written, sizeof(context_header) - amt_written);
			amt_written += temp;
			//printf(strerror(errno));
			//printf("CONTEXT HEADER BEING WRITTEN, amt written = %d, Size of context = %d \n",amt_written, sizeof(context_header));
		}
		//write context
		amt_written = 0;
		while(amt_written < sizeof(context)){
			int temp = write(save_file_fd, (char*)&context + amt_written, sizeof(context) - amt_written);
			amt_written += temp;
			//printf("CONTEXT BEING WRITTEN, amt written = %d, Size of context = %d \n",amt_written, sizeof(context));
		}


		close(save_file_fd);
	}	
	
	//read from proc/self/maps then use write() to write to a file/files? ckpt_segment for each line in proc map.
	//open returns integer fd(to let us keep track of where in the file we are)
	//every time scanf is called we increase the offset so the next scanf reads the next part of the file. the fd is just an integer that the OS stores to keep track of where it is
	//use scanf to read from proc files which can store info in the struct. 
	//if we see permission p in the proc line we can skip it as says in step 8 of readme
	//struct (MY STRUCT TYPE, in this case proc maps) ARRAY_NAME creates array of struct. example code uses array of procmap lines
	//(figure out how to retrieve data from memory)
	//use write to write the data at the memory pointers? write takes an fd(a file we want to store our data in in this case prob ckpt file) and writes from memory addresses through buffer?
	//store each header on the procmap line with the data like descibed in readme in segments but all in one file. 
	//save context using getcontext() and store that in final ckpt segment.
	//store data in some way in file, example was saving header with info like start,end(of memory) read,write execute permissions, if its the context, size of data, alongside the data example was store like Header1, data1, header2, data2 

}
void __attribute__((constructor))
my_constructor(){
	signal(SIGUSR2,&signal_handler);
	printf("The constructor has been run\n");

}
