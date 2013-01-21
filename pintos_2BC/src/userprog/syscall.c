#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	/*********************************************
	Implementing system calls for project 2B
	*********************************************/

	struct thread *cur = thread_current();
	uint32_t *valid = lookup_page(cur->pagedir, f->esp, false);

	if(valid == NULL)
	{
		//printf("\nwrong address\n");
		f->eax = -1;
		printf("%s: exit(%d)\n", cur->name, -1);
		thread_exit();
	}

	int syscall = * (int *)(f->esp);
	
	switch(syscall)
	{
		case(SYS_HALT):
		{	
			power_off();
			break;
		}	
		case(SYS_EXIT):
		{
			int status = * (int *) (f->esp + 4);
			f->eax = status;
			printf("%s: exit(%d)\n", cur->name, status);			
			thread_exit();	
			break;
		}	
		case(SYS_WRITE):
		{
			int file_no = * (int *) (f->esp + 4);
			char *buffer = * (char **) (f->esp + 8);
			if(lookup_page(cur->pagedir, buffer, false) == NULL)
			{
				//printf("\ninvalid address\n");
				f->eax = -1;
				printf("%s: exit(%d)\n", cur->name, -1);
				thread_exit();
			}
			int size = * (int *) (f->esp + 12);
	
			if(file_no == 1)
			{
				putbuf(buffer, size);
			} 

			break;
		}
		case(SYS_EXEC):
		{
			char *process = * (char **) (f->esp + 4);

			if(lookup_page(cur->pagedir, process, false) == NULL)
			{
				//printf("\ninvalid address\n");
				f->eax = -1;
				printf("%s: exit(%d)\n", cur->name, -1);
				thread_exit();
			}
			f->eax = process_wait(process_execute(process));
			break;
		}
		case(SYS_WAIT):
		{
			int tid = * (int *) (f->esp + 4);
			f->eax = process_wait(tid);		
			break;
		}
		case(SYS_CREATE):
		{
			char *file = * (char **) (f->esp + 4);
			if(lookup_page(cur->pagedir, file, false) == NULL)
			{
				//printf("\ninvalid address\n");
				f->eax = -1;
				printf("%s: exit(%d)\n", cur->name, -1);
				thread_exit();
			}
			int file_size = * (int *) (f->esp + 8);
			f->eax = filesys_create(file, file_size);
			break;
		}
		case(SYS_REMOVE):
		{
			char *file = * (char **) (f->esp + 4);
			if(lookup_page(cur->pagedir, file, false) == NULL)
			{
				//printf("\ninvalid address\n");
				f->eax = -1;
				printf("%s: exit(%d)\n", cur->name, -1);
				thread_exit();
			}
			f->eax = filesys_remove(file);
			break;
		}
		case(SYS_OPEN):
		{
			printf("open\n");
			break;
		}
		case(SYS_FILESIZE):
		{
			printf("filesize\n");
			break;
		}
		case(SYS_READ):
		{
			printf("reAD\n");
			break;
		}
		case(SYS_SEEK):
		{
			printf("seek\n");
			break;
		}
		case(SYS_TELL):
		{
			printf("tell\n");
			break;
		}
		case(SYS_CLOSE):
		{
			printf("close\n");
			break;
		}
	}
	
	
	
	
	//---------added code ends-------------------//
  //printf ("system call!\n");
  //thread_exit ();
}
