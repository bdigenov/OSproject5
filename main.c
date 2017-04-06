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
    struct node_t *next;
};

//function called when the frames aren't filled up yet
void push_front(int page, struct node_t **head){
	struct node_t *tmp = malloc(sizeof(struct node_t));
	tmp->page = page;
	tmp->next = *head;
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
int num_disk_write; //Counts number of writes to disk
int num_disk_read;	//Counts number of reads from disk
int page_fault;		//Counts number of page faults 


int headset;
struct node_t *head = NULL;

void print_list(){
	struct node_t *curr = head;
	while(curr != 0){
		printf("%i, ", curr->page);
		curr = curr->next;
	}
	printf("\n");
}

void page_fault_handler( struct page_table *pt, int page ){
	int bits;
	int frame;
	page_fault++;

	page_table_get_entry(pt, page, &frame, &bits);
	if (bits == 0) { 
		//Fill up page table initially
		if (fill_count < nframes){
			page_table_set_entry(pt, page, fill_count, PROT_READ); 
			frames[fill_count] = page;
			disk_read(disk, page, &physmem[fill_count*PAGE_SIZE]);
			num_disk_read++;
			fill_count++;
			if(!strcmp(algorithm,"custom")) { //if we are using the custom algorithm we want to fill up the linked list and make it the right size
				if(head == NULL){
					head = malloc(sizeof(struct node_t));
					headset = 1;
					head->page = page;
					head->next = 0;
				} else {
					
					push_front(page, &head);
				}
				
			}
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
				return;
			}
			
			else if(!strcmp(algorithm,"custom")) {
				int replacepage;
				struct node_t *curr = head;					//iterator over the linked list
				struct node_t *currfollow = curr;			//iterate over half the linked list
				struct node_t *tmp = malloc(sizeof(*tmp));	//create a temporary node of the right size
				int half = 0;
				if(nframes == 1){							//if there is only one page, we have to constantly change the head
					replacepage = head->page;
					free(head);
					
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
					tmp->next = 0;
					head = tmp;							//head reset to new page/frame combo
				} else if(nframes == 2){					//if there is 2 pages, make the new node the head and the old head the second value
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
					tmp->next = head;
					head = tmp;
				} else {
					while(curr->next != 0){							//if there are more than 2 frames, we want to place the new frame in the
						if(curr->next->next == 0){					//middle of the linked list, and kick out the last frame in the list
							replacepage = curr->next->page;			//so we iterate to the last value and find what page it is
							free(curr->next);
							curr->next = 0;
							
							page_table_get_entry(pt, replacepage, &frame, &bits);		//use that page number to find the frame to replace
							if(bits == 3){
								disk_write(disk, replacepage, &physmem[frame*PAGE_SIZE]);
								num_disk_write++;
							}
							disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
							num_disk_read++;
							page_table_set_entry(pt, page, frame, PROT_READ);
							page_table_set_entry(pt, replacepage, frame, 0);
							
							tmp->page = page;						//tmp gets the new values and is placed in the middle of the linked list
							tmp->next = currfollow->next;
							currfollow->next = tmp;
							break;					
						}
						curr = curr->next;
						if(half){
							currfollow = currfollow->next;			//ensures that the currfollow iterator increments only half the time
							half = 0;
						} else {
							half = 1;
						}
					}
				}
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
		page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE); //If pagefault was lack of write access, set write access 
		
		if(!strcmp(algorithm,"custom")){	//if the algorith is custom, we want to move the written frame to the front of the linked list
			struct node_t *curr = head;		// this will simulate a preference for frames that are written to 
			struct node_t *tmp = malloc(sizeof(*tmp));
			if(curr->page != page){
				while(curr != 0){ // move used frame to the front of the linked list
					if(curr->next != 0){
						if(curr->next->page == page){
							tmp->page = page;
							tmp->next = curr->next->next;
							free(curr->next);
							curr->next = tmp->next;
							tmp->next = head;
							head = tmp;
							break;
						}
					} 
					
					curr = curr->next;
				}
			}
		}
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

	// Make the array bigger
	int* more_frames = realloc(frames, nframes * sizeof(int));
	frames = more_frames;
	
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
	//printf("%d, %d, %d, %d\n", nframes, page_fault, num_disk_read,num_disk_write);
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
