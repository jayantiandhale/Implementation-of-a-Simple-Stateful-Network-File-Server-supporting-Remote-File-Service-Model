#include "ssnfs.h"
#include "ssnfs.h"
#include <unistd.h>
#include <stdio.h> // For printf, etc.
#include <rpc/rpc.h> // For RPC facilities.
#include <string.h> // For strcpy, strcmp, strdup, strlen, etc.

/*CONSTANTS DEFINITION*/

/* Enough blocks to store 10 files per user. The first block is used as initial
index once the file system is built */
#define MAX_BLOCKS 6401 
/*Defined by handout*/
#define BLOCK_SIZE 512 
/*file size is fixed*/
#define DEFAULT_BLOCKS 64
/*Assumptions I made for the file system*/
#define MAX_USERS 10
#define MAX_FILES_PER_USER 10
#define MAX_FILE_SIZE 32768
#define MAX_FILE_TABLE_ENTRIES 100
/*STRUCTURES FOR HANDLING THE FILE SYSTEM*/

typedef struct {
	char file_name[10];
	int blocks[DEFAULT_BLOCKS]; 
} node;

typedef struct {
	char user_name[10];
	node files[MAX_FILES_PER_USER];
} user;

typedef struct {
	char alloc_blocks[MAX_BLOCKS+1];
	user users[MAX_USERS];
} metadata;

typedef struct {
	char b_data[BLOCK_SIZE+1];
} block;

typedef struct {
	char file_name[10];
	char user_name[10];
	int offset;
	int fd;
}file_table_record;
struct File_Table{
	file_table_record ft_rec[MAX_FILE_TABLE_ENTRIES];
};
typedef struct File_Table file_table;	

/*GLOBAL VARIABLES*/
const char *disk_name = "virtual.disk";
metadata fs;
block data[MAX_BLOCKS];
struct File_Table *ft=NULL;
int count=100;	//variable for assigning a  file descriptors
/*SERVER-ONLY FUNCTIONS*/


/*Initializes file table structure. All users are named USERNAME and each file is 
named FILENAME by default. */
void init_ft(){
	int i;
	ft=(struct File_Table *)malloc(sizeof(struct File_Table)+1);
	//ft=(file_table *)malloc(sizeof(file_table*MAX_FILE_TABLE_ENTRIES));
	for (i=0; i<MAX_FILE_TABLE_ENTRIES; i++){
		strcpy(ft->ft_rec[i].user_name,"USERNAME");
		strcpy(ft->ft_rec[i].file_name,"FILENAME");
		ft->ft_rec[i].offset=0;
		ft->ft_rec[i].fd=0;
	}
}

/*Initializes metadata structure. All users are named USERNAME and each file is 
named FILENAME by default. */
void init_fs(metadata *filesystem){
	int i,j,k;
	/*All blocks are free once the file system is initialized. */
	memset(filesystem->alloc_blocks,'0',MAX_BLOCKS);
	for (i=0; i<MAX_USERS; i++){
		strcpy(filesystem->users[i].user_name,"USERNAME");
		for(j=0; j<MAX_FILES_PER_USER; j++){
			strcpy(filesystem->users[i].files[j].file_name,"FILENAME");
			for (k=0;k<DEFAULT_BLOCKS;k++)
				filesystem->users[i].files[j].blocks[k]=0;
			}
	}
	/*All data is empty*/
	for (i=0; i<MAX_BLOCKS; i++)
		memset(data[i].b_data,' ', BLOCK_SIZE);
}
/*Writes the actual metadata and data to disk in such a way that allows to be
read if the server crashes and needs to be restarted.*/
void write_metadata(){
	int i,j,k;
	FILE *disk;
	disk = fopen(disk_name, "w");
	/*Writes metadata*/
	fputs(fs.alloc_blocks,disk);
	fputs("\n",disk);
	for (i=0; i<MAX_USERS; i++){
		fputs(fs.users[i].user_name,disk);
		fputs("\n",disk);
		for(j=0; j<MAX_FILES_PER_USER; j++){
			fputs(fs.users[i].files[j].file_name,disk);
			fputs("\n",disk);
			for (k=0;k<DEFAULT_BLOCKS;k++){
				fprintf(disk,"%i ",fs.users[i].files[j].blocks[k]);
			}
			fputs("\n",disk);
		}
	}
	/*End-of-metadata delimiter*/
	fputs("end\n",disk);
	/*Writes data*/
	for (i=0; i<MAX_BLOCKS; i++){
		fputs(data[i].b_data,disk);
		fputs("\n",disk);
	}
	fclose(disk);
}

/*Checks if the disk "disk_name" exists or not. If server crashes, it can 
restore already created file system information.*/
void disk_init(){
	int i,j,k,len;
	FILE *disk;
	char buff[10]; /*Auxiliar buffer that holds \n characters and delimiter*/
	
	if(ft==NULL)
		init_ft();
	int result = access (disk_name,F_OK);  
	
	/*If the disk exists, loads information in file into data and metadata 
	structures*/
	if (result==0){
		disk = fopen(disk_name, "r"); 
		/*Reads metadata*/
		fgets(fs.alloc_blocks,MAX_BLOCKS+1,disk);
		fgets(buff,10,disk);
		for (i=0; i<MAX_USERS; i++){
			fgets(fs.users[i].user_name,10,disk);
			len=strlen(fs.users[i].user_name);
			if (fs.users[i].user_name[len-1] =='\n') /*Avoids newline char*/
				fs.users[i].user_name[len-1]=0;
			for(j=0; j<MAX_FILES_PER_USER; j++){
				fgets(fs.users[i].files[j].file_name,10,disk);
				len=strlen(fs.users[i].files[j].file_name);
				if (fs.users[i].files[j].file_name[len-1] =='\n')
					fs.users[i].files[j].file_name[len-1] = 0;
				for (k=0;k<DEFAULT_BLOCKS;k++)
					fscanf(disk,"%i ",&fs.users[i].files[j].blocks[k]);
			}
		}
		/*Reaches 'end' delimiter*/
		fgets(buff,10,disk);
		/*Reads data from disk file*/
		for (i=0;i<MAX_BLOCKS;i++){
			fgets(data[i].b_data,BLOCK_SIZE+1,disk);
			fgets(buff,10,disk);
		}
	fclose(disk);
	}
	/*If the disk does not exist, initializes file system and builds the disk
	file*/
	else 
	{ 
		init_fs(&fs);
		write_metadata();
	}
}

/*Checks if the user exists in the file system*/
int user_name_exists (char *user_name){
	int i;
	for (i=0;i<MAX_USERS;i++){
		if (strcmp(fs.users[i].user_name,user_name) == 0)
			return 1;
	}
	return 0;
}
/*Returns user name index in the file system's list of users*/
int user_name_index(char *user_name){
	int i;
	for (i=0;i<MAX_USERS;i++){
		if (strcmp(fs.users[i].user_name,user_name) == 0){
			return i;
		}
	}
	return -1;
}
/*Returns file name index in the file system's list of files for an specific 
user*/
int file_name_index(char *user_name, char *file){
	int i;
	int index = user_name_index (user_name);
	for (i=0;i<MAX_FILES_PER_USER;i++){
		if (strcmp(fs.users[index].files[i].file_name,file) == 0){
			return i;
		}
	}
	return -1;
}
/*Checks if a file exists for a given user*/
int file_exists (char *user_name, char *file_name){
	int i; 
	int index = user_name_index (user_name);
	for (i=0;i<MAX_FILES_PER_USER;i++){
		if(strcmp(fs.users[index].files[i].file_name,file_name)==0)
			return 1;
	}
	return 0;
}
/*Returns the number of files that a user owns*/
int number_of_files (char *user){
	int index = user_name_index (user);
	int count=0;
	int i;
	for (i=0;i<MAX_FILES_PER_USER;i++){
		if(strcmp(fs.users[index].files[i].file_name,"FILENAME")!=0)
			count++;
	}
	return count;
}
/*Given an index list, assigns the specified amount of block indexes to that 
list*/
void assign_blocks(int *blocks, int amount){
	int count = 0;
	int b_index=0;
	int i=1;
	/*Checks if the index list already has blocks allocated*/
	while (blocks[b_index] != 0){
		b_index++;
	}

	while (count < amount && i < MAX_BLOCKS){
		/*Assigns the first free block found*/
		if (fs.alloc_blocks[i] == '0'){
			blocks [b_index]=i;
			fs.alloc_blocks[i] = '1';
			count++;
			b_index++;
		}
		i++;
	}
}
/*Given an index list, erases the specified amount of block indexes from that
list. Erases all data pointed to each deallocated block*/
void deallocate_blocks(int *blocks, int amount){
	int i;
	for(i=0; i<amount;i++){
		if(blocks[i]!=0){
			memset(data[blocks[i]].b_data, ' ', BLOCK_SIZE);
			fs.alloc_blocks[blocks[i]] = '0';
			blocks[i] = 0;
		}
	}
}
/*Given an user, adds the specified file name to its home directory. Addition-
ally allocates the DEFAULT_BLOCKS amount for a new file. */
void add_file (char *user, char *file){
	int index = user_name_index (user);
	int i;
	for (i=0;i<MAX_FILES_PER_USER;i++){
		if(strcmp(fs.users[index].files[i].file_name,"FILENAME")==0){
			strcpy(fs.users[index].files[i].file_name,file);
			assign_blocks(fs.users[index].files[i].blocks,DEFAULT_BLOCKS);
			return; 
		}
	}
}


/*Given an user name and a file name, deallocates its blocks and sets its name
to the default value (FILENAME)*/
void remove_file (char *user, char *file){
	int index = user_name_index (user);
	int findex = file_name_index (user, file);
	int i,len;
	strcpy(fs.users[index].files[findex].file_name,"FILENAME");
	deallocate_blocks(fs.users[index].files[findex].blocks,DEFAULT_BLOCKS);
}
/*Returns how mane users are created in the file system*/
int number_of_users(){
	int count=0;
	int i;
	for (i=0;i<MAX_USERS;i++){
		if(strcmp(fs.users[i].user_name,"USERNAME")!=0)
			count++;
	}
	return count;
}
/*Given an user name, looks for the first spot to allocate a new user in the 
file system and adds it.*/
void add_user (char *user){
	int i;
	for (i=0;i<MAX_USERS;i++){
		if(strcmp(fs.users[i].user_name,"USERNAME")==0){
			strcpy(fs.users[i].user_name,user);
			return; 
		}
	}
}

/*Given an user name, stores the list of files different from the default name 
in a buffer.*/
void all_files (char *user, char *buffer){
	int index = user_name_index (user);
	strcpy(buffer,"Files: ");
	int i;
	for (i=0;i<MAX_FILES_PER_USER;i++){
		if(strcmp(fs.users[index].files[i].file_name,"FILENAME")!=0){
			strcat(buffer,fs.users[index].files[i].file_name);
			strcat(buffer," ");}
	} 
}

/*
Add entry to the file_table if record doesn't exist
*/
int add_file_table(char *user, char *file){
	int index = user_name_index (user);
	int i;
	for (i=0;i<MAX_FILE_TABLE_ENTRIES;i++){
			if(strcmp(ft->ft_rec[i].user_name,"USERNAME")==0 && strcmp(ft->ft_rec[i].file_name,"FILENAME")==0){
				strcpy(ft->ft_rec[i].file_name,file);
				strcpy(ft->ft_rec[i].user_name,user);
				ft->ft_rec[i].offset=0;
				ft->ft_rec[i].fd=count++;
				return ft->ft_rec[i].fd; 
		}
	}
	return -1;
}
/*Checks if a record exists in file table for a given user and file name*/
int record_exists_file_table(char *user_name, char *file_name){
	int i; 
	for (i=0;i<MAX_FILE_TABLE_ENTRIES;i++){
		if(strcmp(ft->ft_rec[i].file_name,file_name)==0 && strcmp(ft->ft_rec[i].user_name,user_name)==0)
			return i;
	}
	return -1;
}
/*Checks if file descriptor is a valid number */
int isValidFd(int fd){
	if(fd>0)
		return 1;
	printf("\nInvalid file descriptor %d!",fd);
	return -1;
}
/*Checks if file descriptor already exists in the file table and returns the file table index*/
int file_descriptor_exists(int fd){
	int i=-1; 
	if(isValidFd(fd)){
		for (i=0;i<MAX_FILE_TABLE_ENTRIES;i++){
			if(ft->ft_rec[i].fd==fd)
				return i;
		}
	}
	return -1;
}
/*prints contents of file table*/
void print_file_table(){
        int i;
                printf("\nfile table:");
                printf("\nUserName\tfileName\toffset\tfileDescriptor");

        for (i=0;i<MAX_FILE_TABLE_ENTRIES;i++){
                        if(strcmp(ft->ft_rec[i].user_name,"USERNAME")!=0 && strcmp(ft->ft_rec[i].file_name,"FILENAME")!=0){
                                printf("\n%s\t\t%s\t\t%d\t\t%d",ft->ft_rec[i].user_name,ft->ft_rec[i].file_name,ft->ft_rec[i].offset,ft->ft_rec[i].fd);
                        }
                }

}

/*updates the offset for given file table and user*/
void update_file_table(char *user_name,int fd,int b_offset){
	int i;
	if(isValidFd(fd)){
		for (i=0;i<MAX_FILE_TABLE_ENTRIES;i++){
			if(ft->ft_rec[i].fd==fd && strcmp(ft->ft_rec[i].user_name,user_name)==0){
				ft->ft_rec[i].offset=b_offset;
				print_file_table();
				return;
			}
		}
	}
}
/*removes entry for file descriptor from file table*/
void remove_ft_entry(int fd){
	int cnt=0,i;
	if(isValidFd(fd)){
		for (i=0;i<MAX_FILE_TABLE_ENTRIES;i++){
			if(ft->ft_rec[i].fd==fd){
				strcpy(ft->ft_rec[i].file_name,"FILENAME");
				strcpy(ft->ft_rec[i].user_name,"USERNAME");
				ft->ft_rec[i].offset=0;
				ft->ft_rec[i].fd=0;
				cnt++;
			}
		}
		printf("\nTotal %d file table entries are removed",cnt);
	}
}
/*FUNCTIONS GENERATED BY RPCGEN TEMPLATE - USED BY CLIENT*/

/*
Opens a file with given file name if present; else
Creates a file in the file system for a user. If the user does not exist
in the file system, adds it if possible. If the file is created updates the 
disk and file table with the new information. 

Errors handled:
- The user already has the maximum number of files allowed (10)
- The file system already has the maximum number of users allowed (10)
*/
open_output *
open_file_1_svc(open_input *argp, struct svc_req *rqstp)
{
	static open_output  result;
	char buffer[255];
	int ft_index=0,fd=0;
	/*Initializes the disk or loads data from an already created one. */
	disk_init(); 
	ft_index=record_exists_file_table(argp->user_name, argp->file_name);
	if(ft_index!=-1){
		printf("\nOPEN:file descriptor for open file is:%d",fd);
		result.fd=ft->ft_rec[ft_index].fd;
		return &result;
	}else{
	/*If already file is not already created->create a file and open*/
	if (user_name_exists (argp->user_name)){
		if (file_exists (argp->user_name, argp->file_name)){
				fd=add_file_table(argp->user_name,argp->file_name);
		} else if (number_of_files(argp->user_name)>=MAX_FILES_PER_USER){
			sprintf(buffer,"ERROR-OPEN: no space available for file '%s'", 
				argp->file_name);
		} else {
			add_file (argp->user_name, argp->file_name);
			sprintf(buffer, "OPEN:FILE '%s' CREATED FOR USER %s",argp->file_name,
				argp->user_name );
			write_metadata();
			printf("\nOPEN:File created\n");
			fd=add_file_table(argp->user_name,argp->file_name);
		}
	}
	else if (number_of_users() >= MAX_USERS) {
		sprintf(buffer, "ERROR-OPEN: no space available for user '%s'",
			argp->user_name);
	} 
	else {
		add_user (argp->user_name);
		add_file (argp->user_name, argp->file_name);
		sprintf(buffer, "OPEN:FILE '%s' CREATED FOR USER %s",argp->file_name,
			argp->user_name );
		write_metadata();
		printf("OPEN:File created\n");
		fd=add_file_table(argp->user_name,argp->file_name);
	}
}	
/*Message returned to client*/
	if(fd==-1){
			printf("\nOPEN:Maximum file table limit limit is reached! Cannot add entry to file table!");
			exit(1);
	}
	printf("\nOPEN:File descriptor for opened file is:%d",fd);
	result.fd=fd;
	return &result;
}

/*Lists all the files in the home directory for a given user name. 
Errors handled:
- The user does not have files. 
- The user does not exist in the file system. 
*/
list_output *
list_files_1_svc(list_input *argp, struct svc_req *rqstp)
{
	static list_output  result;
	char buffer[255];
	/*Initializes the disk or loads data from an already created one. */
	disk_init();
	/*List function validations*/
	if (user_name_exists (argp->user_name)){
		if (number_of_files(argp->user_name)==0){
			sprintf(buffer,"\nLIST:USER '%s' DOES NOT HAVE FILES", argp->user_name);
		} else {
			all_files(argp->user_name, buffer);
			printf("\nLIST:Listed files for user. \n");
		}
	}
	else {
		sprintf(buffer, "ERROR-LIST: user '%s' does not exist ",argp->user_name );
	}	
	/*Message returned to client*/
	result.out_msg.out_msg_len = strlen(buffer);
	result.out_msg.out_msg_val = malloc(result.out_msg.out_msg_len);
	strcpy(result.out_msg.out_msg_val,buffer);
	return &result;
}

/*Given a file name and an user name, deletes the file from the user's home 
directory. Upon completion, the user remains in the file system and the data
is written to disk. 

Errors handled:

- The requested file to delete does not exist.
- The user that is requesting the operation does not exist in the file system. 

*/

delete_output *
delete_file_1_svc(delete_input *argp, struct svc_req *rqstp)
{
	static delete_output  result;
	char buffer[255];
	char *file_name;
	int ft_index=0,fd=0;
	if(record_exists_file_table(argp->user_name, argp->file_name)!=-1){
		ft_index=record_exists_file_table(argp->user_name, argp->file_name);
		file_name=ft->ft_rec[ft_index].file_name;
		remove_ft_entry(ft->ft_rec[ft_index].fd);
	}else{
		printf("\nDELETE: No entry in file table found for file descriptor %d!",fd);
	}

	/*Initializes the disk or loads data from an already created one. */
	disk_init();
	/*List function validations*/

	if (user_name_exists (argp->user_name)){
		if (!file_exists (argp->user_name,argp->file_name)){
			sprintf(buffer,"ERROR-DELETE: file '%s' does not exist",argp->file_name);
		}  
		else {
			remove_file (argp->user_name, argp->file_name);
			sprintf(buffer, "DELETE:FILE '%s' DELETED FOR USER %s",argp->file_name,
				argp->user_name );
			write_metadata();
			printf("DELETE:File deleted.\n");
		}
	}
	else {
		sprintf(buffer, "ERROR-DELETE: user '%s' does not exists ",argp->user_name );
	}
	/*Message returned to client*/
	result.out_msg.out_msg_len = strlen(buffer);
	result.out_msg.out_msg_val = malloc(result.out_msg.out_msg_len);
	strcpy(result.out_msg.out_msg_val,buffer);
	return &result;
}

/*Writes the given number of bytes of the given buffer from the specified position
of the given file name that belongs to the given user name. Upon completion,
the data is written to disk. 

Errors handled:
- The file is not open to write for given user
- Trying to write after the maximum file size (64 Blocks).
- The user that is requesting the operation does not exist in the file system. 
- The file does not exist in the user's home directory.  

*/
write_output *
write_file_1_svc(write_input *argp, struct svc_req *rqstp)
{
	int j,block_index,uindex,findex,actual_size,neccesary_blocks,b_offset;
	/*Initializes the disk or loads data from an already created one. */
	disk_init();
	char buffer[255];
	int block,last_byte_pos,offset=-1,ft_index=0;
	char *file_name;
	//ft_index=file_descriptor_exists(argp->fd);
	if(isValidFd(argp->fd)!=-1 &&  file_descriptor_exists(argp->fd)!=-1){
			ft_index=file_descriptor_exists(argp->fd);

			file_name=ft->ft_rec[ft_index].file_name;
			offset=ft->ft_rec[ft_index].offset;
	
	/*Last byte position to be written in the entire file*/
	last_byte_pos = offset + argp->numbytes-1; 
	/*Sets initial block where the first byte might be written*/
	block =offset/BLOCK_SIZE;
	if (user_name_exists (argp->user_name)){
		/*Trying to write after maximum end of file*/
		if ( last_byte_pos > (BLOCK_SIZE * DEFAULT_BLOCKS)) {
			sprintf(buffer,
			"ERROR-WRITE: trying to write after end of file. Max file size exceeded");	
		}
		else if(file_exists(argp->user_name, file_name)){
			uindex = user_name_index(argp->user_name);
			findex = file_name_index(argp->user_name, file_name);
			/*Computes the starting block index*/
			b_offset = (offset)%BLOCK_SIZE;
			/*Writes the specified bytes*/
			for (j=0;j<argp->numbytes;j++){
				/*If the last position of the current block is filled, 
				continues writing in the beginning of the next block.*/
				if (b_offset==BLOCK_SIZE){
					block++;
					b_offset=0;
				}
				/*Computes block to be modified index. Checks if the 
				current block belongs to the default assigned blocks*/
				if(block<DEFAULT_BLOCKS){
					 block_index = fs.users[uindex].files[findex].blocks[block];
			
                        	      /*Writes the current byte to the block*/
                                	data[block_index].b_data[b_offset]=argp->buffer.buffer_val[j];
	                                b_offset++;
					continue;
				}else{
					sprintf(buffer,"\nWRITE:Trying to write after end of file. Max file size exceeded");
					break;
				}	
			}
			sprintf(buffer,"\nWRITE: %i characters written to '%s' ",argp->numbytes,file_name);
			/*Upon completion, writes data to disk*/
			write_metadata();
			printf("\nFile modified.\n");
			update_file_table(argp->user_name,argp->fd,b_offset);
		} else {
			sprintf(buffer, "\nERROR: file '%s' does not exist ",file_name);
		}
	} 
	else 
	{
		sprintf(buffer, "\nERROR: user '%s' does not exist ", argp->user_name);
	}
	}else{
		sprintf(buffer,"\nInvalid file descriptor %d!",argp->fd);

	}
	static write_output  result;
	/*Message returned to client*/
	result.out_msg.out_msg_len = strlen(buffer);
	result.out_msg.out_msg_val = malloc(result.out_msg.out_msg_len);
	strcpy(result.out_msg.out_msg_val,buffer);
	return &result;
}

/*Reads the given number of bytes from the specified position of the given file 
name that belongs to the given user name. 

Errors handled:
- The file is not open when user tries to read
- Trying to read after the actual file size (end of file).
- The user that is requesting the operation does not exist in the file system. 
- The file does not exist in the user's home directory.  

*/
read_output *
read_file_1_svc(read_input *argp, struct svc_req *rqstp)
{
	int j,block_index,uindex,findex,actual_size,b_offset;
	/*Initializes the disk or loads data from an already created one. */
	disk_init();
	char buffer[255];
	char aux[(argp->numbytes) + 1]; /*Stores bytes read*/
	/*Sets initial block where the first byte might be written*/
	int offset=0,block=0,ft_index=0;
	char file_name[10];
	if(isValidFd(argp->fd)!=-1 &&  file_descriptor_exists(argp->fd)!=-1){
                        ft_index=file_descriptor_exists(argp->fd);
			offset=ft->ft_rec[ft_index].offset;
	

	block=(offset)/BLOCK_SIZE;
	/*Last byte position to be read in the entire file*/
	int last_byte_pos = offset + argp->numbytes;
	if (user_name_exists (argp->user_name)){
		if(file_exists(argp->user_name, ft->ft_rec[ft_index].file_name)){	
			uindex = user_name_index(argp->user_name);
			findex = file_name_index(argp->user_name, ft->ft_rec[ft_index].file_name);
			actual_size = DEFAULT_BLOCKS;
			
			if (last_byte_pos > actual_size * BLOCK_SIZE){
				sprintf(buffer,"ERROR: trying to read after end of file.");	
			} 
			else{
				strcpy(buffer,"\nContent: ");
				/*Computes the starting block index*/
				b_offset = (offset)%BLOCK_SIZE;
				/*Reads the specified bytes*/
				for (j=0;j<argp->numbytes;j++){
					/*If the last byte read is at the end of a block, continue
					reading at the beginning of the next block*/
					if (b_offset==BLOCK_SIZE){
						block++;
						b_offset=0;
					}
					/*Computes block to be read index. Checks if the current 
					block belongs to the default assigned blocks or the 
					extra blocks*/
					if(block<DEFAULT_BLOCKS){
						block_index = fs.users[uindex].files[findex].blocks[block];
				
					/*Stores the byte read in an auxiliar buffer*/
					aux[j]=data[block_index].b_data[b_offset];
					b_offset++;
					
					}else{
					break;
	}
		}
		
				aux[j]='\0';
				strcat(buffer,aux); /*Prepares response to client*/
				printf("\nCharacters read.\n");	
				update_file_table(argp->user_name,argp->fd,b_offset);

			}	
		} else {
			sprintf(buffer, "ERROR - file '%s' does not exist ",ft->ft_rec[ft_index].file_name);
		}
	} 
	else {
		sprintf(buffer, "ERROR-READ: user '%s' does not exist ", argp->user_name);
	}
	}else{
		sprintf(buffer,"\nERROR - READ: No entry in file table found for file descriptor %d for user '%s'!",argp->fd, argp->user_name);
	}
	static read_output  result;
	/*Message returned to client*/
	result.out_msg.out_msg_len = strlen(buffer);
	result.out_msg.out_msg_val = malloc(result.out_msg.out_msg_len);
	strcpy(result.out_msg.out_msg_val,buffer);
	return &result;

}

/*The file is closed for given user. It removes the entry of file for given user name from file table.

Errors handled:
- File is not open to close.
*/

close_output *
close_file_1_svc(close_input *argp, struct svc_req *rqstp)
{
	char buffer[255];
	char *file_name;
	int ft_index=0;

	 if(isValidFd(argp->fd)!=-1 &&  file_descriptor_exists(argp->fd)!=-1){
                ft_index=file_descriptor_exists(argp->fd);
		remove_ft_entry(argp->fd);	
		sprintf(buffer,"\nCLOSE: File with file descriptor %d is closed for you!\n",argp->fd);
	}else{
		sprintf(buffer,"\nCLOSE:File is NOT OPEN with file descriptor %d!",argp->fd);
		printf("\nCLOSE:File is NOT OPEN with file descriptor %d!\n",argp->fd);
	}	
	/*Message returned to client*/
	static close_output  result;
	result.out_msg.out_msg_len = strlen(buffer);
	result.out_msg.out_msg_val = malloc(result.out_msg.out_msg_len);
	strcpy(result.out_msg.out_msg_val,buffer);
	return &result;
}

