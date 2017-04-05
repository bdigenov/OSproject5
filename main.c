/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>


//Define struct for keeping track of page/frame pairs
struct node_t {
    int page;
	int written;
    struct node_t *next;
	//struct node_t *prev;
};

void push_front(int page, struct node_t **head){
	//printf("int push_front with page %i\n", page);
	struct node_t *tmp = malloc(sizeof(struct node_t));
	tmp->page = page;
	tmp->written = 0;
	tmp->next = *head;
	//tmp->prev = 0;
	*head = tmp;
}




//Define Globals
int npages;			//Number of Pages 
int nframes;		//Number of frames 
char *algorithm;	//Algorithm inputted by user
char *program;		//Program inputted by user
struct disk *disk;	//Virtual Disk 
char *virtmem;		//Virtual Memory storage
char *physmem;		//Physical Memory storage
int fill_count;		//Amount of frames that have been filled
int* frames;		//Frame storage
//int* page_values;
int num_disk_write; //Counts number of writes to disk
int num_disk_read;	//Counts number of reads from disk
int page_fault;		//Counts number of page faults 
int writer;

int headset;
struct node_t *head = NULL;
//struct node_t *head = malloc(sizeof(struct node_t));
//head = (struct node_t *) malloc(sizeof(struct node_t));

void print_list(){
	struct node_t *curr = head;
	while(curr != 0){
		printf("%i:%i, ", curr->page, curr->written);
		curr = curr->next;
	}
	printf("\n");
}

void page_fault_handler( struct page_table *pt, int page ){
	int bits;
	int frame;
	page_fault++;
	//printf("in pfh\n");

	page_table_get_entry(pt, page, &frame, &bits);
	//printf("ptge called\n");
	if (bits == 0) { 
		//Fill up page table initially
		if (fill_count < nframes){
			//printf("here\n");
			page_table_set_entry(pt, page, fill_count, PROT_READ); 
			frames[fill_count] = page;
			disk_read(disk, page, &physmem[fill_count*PAGE_SIZE]);
			num_disk_read++;
			fill_count++;
			//page_table_print(pt);
			if(!strcmp(algorithm,"custom")) {
				//printf("in custom\n");
				if(head == NULL){
					head = malloc(sizeof(struct node_t));
					//printf("malloc done \n");
					headset = 1;
					head->page = page;
					head->written = 0;
					head->next = 0;
					//head->prev = 0;
					//printf("head is set\n");
				} else {
					
					push_front(page, &head);
				}
				
			}
			//printf("before return\n");
			return;
		}

		else {
			if(!strcmp(algorithm,"rand")) {
				//Randomly pick frame to send page to 
				int n = rand() % (nframes);

				//Check if the frame getting kicked out has been written to
				page_table_get_entry(pt, frames[n], &frame, &bits);
				if(bits == 3) {
					disk_write(disk, frames[n], &physmem[frame*PAGE_SIZE]);
					num_disk_write++;
					disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
					num_disk_read++;
					page_table_set_entry(pt, page, frame, PROT_READ);
					page_table_set_entry(pt, frames[n], frame, 0);
					frames[n] = page; //Update the page frame tracker 
				}

				else{
					//If it has been written to, write it back to disk 
					page_table_set_entry(pt, page, frame, PROT_READ);
					disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
					num_disk_read++;
					page_table_set_entry(pt, frames[n], frame, 0);
					frames[n] = page; //Update the page frame tracker 
				}
				//printf("PAGE TABLE\n");
				//page_table_print(pt);
				return;
			}
			else if(!strcmp(algorithm,"fifo")) {
				int n = fill_count % nframes;
				//Check if the frame getting kicked out has been written to
				page_table_get_entry(pt, frames[n], &frame, &bits);
				if(bits == 3) {
					disk_write(disk, frames[n], &physmem[frame*PAGE_SIZE]);
					num_disk_write++;
					disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
					num_disk_read++;
					page_table_set_entry(pt, page, frame, PROT_READ);
					page_table_set_entry(pt, frames[n], frame, 0);
					frames[n] = page; //Update the page frame tracker 
				}

				else{
					//If it has been written to, write it back to disk 
					page_table_set_entry(pt, page, frame, PROT_READ);
					disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
					num_disk_read++;
					page_table_set_entry(pt, frames[n], frame, 0);
					frames[n] = page; //Update the page frame tracker 
				}
				fill_count++;
				//printf("PAGE TABLE\n");
				//page_table_print(pt);
				return;
			}
			
			else if(!strcmp(algorithm,"custom")) {
				int replacepage;
				struct node_t *curr = head;
				struct node_t *currfollow = curr;
				struct node_t *tmp = malloc(sizeof(*tmp));
				//int prev = 0;
				//int prevworked = 0;
				//int resethead = 0;
				int half = 0;
				if(nframes == 2){
					replacepage = head->next->page;
					free(head->next);
					head->next = 0;
					page_table_get_entry(pt, replacepage, &frame, &bits);
					if(bits == 3){
						disk_write(disk, replacepage, &physmem[frame*PAGE_SIZE]);
						num_disk_write++;
					}
					disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
					num_disk_read++;
					page_table_set_entry(pt, page, frame, PROT_READ);
					page_table_set_entry(pt, replacepage, frame, 0);
					tmp->page = page;
					tmp->written = 0;
					tmp->next = head;
					head = tmp;
				} else {
					while(curr->next != 0){
						if(curr->next->next == 0){
							replacepage = curr->next->page;
							free(curr->next);
							curr->next = 0;
							
							page_table_get_entry(pt, replacepage, &frame, &bits);
							if(bits == 3){
								disk_write(disk, replacepage, &physmem[frame*PAGE_SIZE]);
								num_disk_write++;
							}
							disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
							num_disk_read++;
							page_table_set_entry(pt, page, frame, PROT_READ);
							page_table_set_entry(pt, replacepage, frame, 0);
							
							tmp->page = page;
							tmp->written = 0;
							//tmp->next = head;
							tmp->next = currfollow->next;
							//tmp->prev = currfollow;
							currfollow->next = tmp;
							//currfollow->next->prev = tmp;
							//head->prev = tmp;
							//tmp->prev = 0;
							//head = tmp;
							//curr->written = 0;
							//curr->next->page = page;
							break;					
						}
						curr = curr->next;
						if(half){
							currfollow = currfollow->next;
							half = 0;
						} else {
							half = 1;
						}
					}
				}
					//printf("PAGE TABLE (non write fault):\n");
					//page_table_print(pt);
					//printf("non write fault changed linked list:\n");
					//printf("prevworked:%i, prev:%i\n", prevworked, prev);
					//print_list();
				return;
			}
			

			else {
				printf("Invalid Algorithm Choice\n");
				printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
				exit(1);
			}
		}
	}

	else if (bits == 1) { //in memory but READ_ONLY - since there was an error it is trying to write - must change permissions 
		//printf("its here with bits = 1\n");
		page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE); //If pagefault was lack of write access, set write access 
		
		if(!strcmp(algorithm,"custom")){
			//printf("within write\n");
			struct node_t *curr = head;
			struct node_t *tmp = malloc(sizeof(*tmp));
			if(curr->page == page){
				writer++;
				//head->written = 1;
			} else {
				while(curr != 0){ // move used frame to the front of the linked list
					if(curr->next != 0){
						if(curr->next->page == page){
							tmp->page = page;
							tmp->next = curr->next->next;
							free(curr->next);
							curr->next = tmp->next;
							tmp->next = head;
							//tmp->prev = 0;
							//head->prev = tmp;
							tmp->written = 1;
							head = tmp;
							break;
						}
					} 
					
					curr = curr->next;
				}
			}
			//head->written = 1;
		}
		//printf("post write-fault: \n");
		//page_table_print(pt);
		//print_list();
		//printf("NO WRITE ACCESS: page fault on page #%d\n",page);
		return;
	}			
	exit(1);
}

int main( int argc, char *argv[] )
{
	headset = 0;
	fill_count = 0;
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}
	srand(time(0));

	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	algorithm = argv[3];
	program = argv[4];
	num_disk_write = 0;
	num_disk_read = 0;
	page_fault = 0;
	writer = 0;

	// Make the array bigger
	int* more_frames = realloc(frames, nframes * sizeof(int));
	frames = more_frames;
	
	/*int* more_page_values = realloc(page_values, npages * sizeof(int));
	page_values = more_page_values;
	int p;
	for(p=0; p<npages; p++){
		page_value[p] = rand()%10;
	}
*/
	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	virtmem = page_table_get_virtmem(pt);

	physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
		return 1;
	}

	printf("PAGE FAULTS: %d\n", page_fault);
	printf("DISK WRITES: %d\n", num_disk_write);
	printf("DISK READS: %d\n",num_disk_read);
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}