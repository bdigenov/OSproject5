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
					page_table_set_entry(pt, page, frame, PROT_WRITE | PROT_READ);
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
					page_table_set_entry(pt, page, frame, PROT_WRITE | PROT_READ);
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
				//Implement random algorithm
				printf("custom\n");
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
		//printf("NO WRITE ACCESS: page fault on page #%d\n",page);
		return;
	}			
	exit(1);
}

int main( int argc, char *argv[] )
{
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
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}