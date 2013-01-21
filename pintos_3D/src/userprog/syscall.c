#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

struct fd * get_fd_by_no(int no);
bool check(int no_of_args, struct intr_frame *f);
void quit(struct intr_frame *f);

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
	if(f->esp == NULL || !is_user_vaddr (f->esp) || valid == NULL)
		quit(f);

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
			if(!check(1, f))
				quit(f);
			int status = * (int *) (f->esp + 4);
			f->eax = status;
			printf("%s: exit(%d)\n", cur->name, status);		
			struct dead_thread *thread_to_die = (struct dead_thread *)malloc(sizeof(struct dead_thread));
			thread_to_die->exit_status = status;
			thread_to_die->tid = thread_current()->tid;
			list_push_front(&thread_current()->parent->dead_child_list, &thread_to_die->elem);
			thread_exit();	
			break;
		}	
		case(SYS_EXEC):
		{
			if(!check(1, f))
				quit(f);
			char *process = * (char **) (f->esp + 4);
			if(process == NULL || !is_user_vaddr (process) || lookup_page(cur->pagedir, process, false) == NULL)
				quit(f);
			f->eax = process_execute(process);
			list_push_front(&thread_current()->child_lock.waiters, &thread_current()->elem);
			thread_block();
			if(thread_current()->child_load_failed == true)
				f->eax = -1;				
			break;
		}
		case(SYS_WAIT):
		{
			if(!check(1, f))
				quit(f);
			int tid = * (int *) (f->esp + 4);
			f->eax = process_wait(tid);
			break;
		}
		case(SYS_CREATE):
		{
			if(!check(2, f))
				quit(f);
			char *file = * (char **) (f->esp + 4);
			if(file == NULL || !is_user_vaddr (file) || lookup_page(cur->pagedir, file, false) == NULL)
				quit(f);
			int file_size = * (int *) (f->esp + 8);
			f->eax = filesys_create(file, file_size);
			break;
		}
		case(SYS_REMOVE):
		{
			if(!check(1, f))
				quit(f);
			char *file = * (char **) (f->esp + 4);
			if(file == NULL || !is_user_vaddr (file) || lookup_page(cur->pagedir, file, false) == NULL)
				quit(f);
			f->eax = filesys_remove(file);
			break;
		}
		case(SYS_OPEN):
		{
			if(!check(1, f))
				quit(f);
			char *file_name = * (char **) (f->esp + 4);
			struct file *file;
			if(file_name == NULL || !is_user_vaddr (file_name) || lookup_page(cur->pagedir, file_name, false) == NULL)
				quit(f);
			file = filesys_open(file_name);
			if(file == NULL)
				f->eax = -1;
			else
			{
				if(!list_empty(&cur->fd_list))
				{
					int prev_fd_no = list_entry(list_back(&cur->fd_list), struct fd, elem)->file_no;
					struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
					new_fd->file_no = prev_fd_no+1;
					new_fd->file = file;
					list_push_back(&cur->fd_list, &new_fd->elem);
					f->eax = new_fd->file_no;
				}
				else
				{
					struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
					new_fd->file_no = 3;
					new_fd->file = file;
					list_push_back(&cur->fd_list, &new_fd->elem);
					f->eax = new_fd->file_no;
				}	
			}
			if(strcmp(file_name, thread_current()->name) == 0 || strcmp(file_name, thread_current()->parent->name) == 0)
			{
				file_deny_write(file);
			}	
			break;
		}
		case(SYS_FILESIZE):
		{
			if(!check(1, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd != NULL)
				f->eax = file_length(cur_fd->file);
			break;
		}
		case(SYS_READ):
		{
			if(!check(3, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);
			char *buffer = * (char **) (f->esp + 8);
			if(buffer == NULL || !is_user_vaddr (buffer) || lookup_page(cur->pagedir, buffer, false) == NULL)
				quit(f);
			int size = * (int *) (f->esp + 12);
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd != NULL)
			{
				f->eax = file_read(cur_fd->file, buffer, size);
			}
			else if(fd_no == 0)
			{
				input_getc();
				f->eax = size;
			} 
			break;
		}
		case(SYS_WRITE):
		{
			if(!check(3, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);
			char *buffer = * (char **) (f->esp + 8);
			if(buffer == NULL || !is_user_vaddr (buffer) || lookup_page(cur->pagedir, buffer, false) == NULL)
				quit(f);
			int size = * (int *) (f->esp + 12);
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd != NULL)
			{
				f->eax = file_write(cur_fd->file, buffer, size);
			}
			else if(fd_no == 1)
			{
				putbuf(buffer, size);
				f->eax = size;
			} 

			break;
		}
		case(SYS_SEEK):
		{
			if(!check(2, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);
			unsigned position = * (unsigned *) (f->esp + 8);
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd != NULL)
				file_seek(cur_fd->file, position);
			break;
		}
		case(SYS_TELL):
		{
			if(!check(1, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd != NULL)
				f->eax = file_tell(cur_fd->file);
			break;
		}
		case(SYS_CLOSE):
		{
			if(!check(1, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd != NULL)
			{
				list_remove(&cur_fd->elem);
				file_close(cur_fd->file);
				free(cur_fd);
			}	
			 			
			break;
		}
	}
	
	//---------added code ends-------------------//
  
}
struct fd * get_fd_by_no(int no)
{
	struct thread *cur = thread_current();
	struct list_elem *e;
	struct fd *cur_fd;
	for(e=list_begin(&cur->fd_list);e!=list_end(&cur->fd_list);e=list_next(e))
	{
		cur_fd = list_entry(e, struct fd, elem);
		if(cur_fd->file_no == no)
			break;
	}
	if(e==list_end(&cur->fd_list))
		return NULL;
	else
		return cur_fd;			
}
bool check(int no_of_args, struct intr_frame *f)
{
	int i;
	for(i=0;i<no_of_args;i++)
	{
		if(!is_user_vaddr (f->esp+4*(i+1)) || lookup_page(thread_current()->pagedir, f->esp+4*(i+1), false) == NULL)
			return false;
	}
	return true;
}
void quit(struct intr_frame *f)
{
	f->eax = -1;
	printf("%s: exit(%d)\n", thread_current()->name, -1);
	thread_exit();
}

