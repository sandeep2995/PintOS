#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"

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

			if(strcmp(file, "/") == 0)
			{
				f->eax = 0;
				return;
			}	
			struct dir *parent_dir = get_dir_from_path(file);
			char *name = get_name_from_path(file);

			struct inode *inode;
			if(!dir_lookup(parent_dir, name, &inode))
			{
				f->eax = 0;
				return;
			}
			struct dir_entry e;
			if(!lookup(parent_dir, name, &e, NULL))
			{
				f->eax = 0;
				return;
			}
			if(e.is_dir == 0)
			{
				
				f->eax = filesys_remove(file);
			}
			else
			{
				struct dir *directory = dir_open(inode);
				char f_name[NAME_MAX + 1];
				if(dir_readdir(directory, f_name))
				{
					f->eax = 0;
					dir_close(directory);
					return;
				}
				if(directory->inode->sector == cur->cur_dir)
				{
					f->eax = 0;
					dir_close(directory);
					return;
				}
				struct list_elem *e;
				struct fd *new_fd;
				for(e=list_begin(&cur->fd_list);e!=list_end(&cur->fd_list);e=list_next(e))
				{
					new_fd = list_entry(e, struct fd, elem);
					if(new_fd->dir->inode->sector == directory->inode->sector)
					{
						f->eax = 0;
						dir_close(directory);
						return;
					}
				}
				f->eax = dir_remove(parent_dir, name);		
				dir_close(directory);				
			}	
			break;
		}
		case(SYS_OPEN):
		{
			if(!check(1, f))
				quit(f);
			char *file_name = * (char **) (f->esp + 4);
			if(file_name == NULL || !is_user_vaddr (file_name) || lookup_page(cur->pagedir, file_name, false) == NULL)
				quit(f);
			char *path = file_name;

			if(strcmp(path, "/") == 0)
			{
				struct dir *dir = dir_open_root();
				if(dir == NULL)
					f->eax = -1;
				else
				{
					if(!list_empty(&cur->fd_list))
					{
						int prev_fd_no = list_entry(list_back(&cur->fd_list), struct fd, elem)->file_no;
						struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
						new_fd->file_no = prev_fd_no+1;
						new_fd->dir = dir;
						new_fd->is_dir = true;
						list_push_back(&cur->fd_list, &new_fd->elem);
						f->eax = new_fd->file_no;
					
					}
					else
					{
						struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
						new_fd->file_no = 3;
						new_fd->dir = dir;
						new_fd->is_dir = true;
						list_push_back(&cur->fd_list, &new_fd->elem);
						f->eax = new_fd->file_no;
					
					}	
					
				}
				return;
			}
			struct dir *cur_dir = get_dir_from_path(path);
			char *name = get_name_from_path(path);
			if(cur_dir == NULL || name == NULL)
			{
				f->eax = -1;
				return;
			}
			struct dir_entry e;
			if(!lookup(cur_dir, name, &e, NULL))
			{
				f->eax = -1;
				return;
			}
			if(!e.is_dir)
			{	
			
				struct file *file;
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
						new_fd->is_dir = false;
						list_push_back(&cur->fd_list, &new_fd->elem);
						f->eax = new_fd->file_no;
					}
					else
					{
						struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
						new_fd->file_no = 3;
						new_fd->file = file;
						new_fd->is_dir = false;
						list_push_back(&cur->fd_list, &new_fd->elem);
						f->eax = new_fd->file_no;
					}	
				}
				if(strcmp(file_name, thread_current()->name) == 0 || strcmp(file_name, thread_current()->parent->name) == 0)
				{
					
					file_deny_write(file);
				}	
			}
			else
			{
				struct dir *dir = dir_open(inode_open(e.inode_sector));
				
				if(dir == NULL)
					f->eax = -1;
				else
				{
					if(!list_empty(&cur->fd_list))
					{
						int prev_fd_no = list_entry(list_back(&cur->fd_list), struct fd, elem)->file_no;
						struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
						new_fd->file_no = prev_fd_no+1;
						new_fd->dir = dir;
						new_fd->is_dir = true;
						list_push_back(&cur->fd_list, &new_fd->elem);
						f->eax = new_fd->file_no;
					
					}
					else
					{
						struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));
						new_fd->file_no = 3;
						new_fd->dir = dir;
						new_fd->is_dir = true;
						list_push_back(&cur->fd_list, &new_fd->elem);
						f->eax = new_fd->file_no;
					
					}	
					
				}	
				
			}		
			dir_close(cur_dir);
			
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
				if(cur_fd->is_dir)
				{
					f->eax = -1;
					return;			
				}
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
		/*********************************************
		Implementing system calls for project 4
		*********************************************/
		case(SYS_CHDIR):
		{
			if(!check(1, f))
				quit(f);
			char *path = * (char **) (f->esp + 4);			
			if(path == NULL || !is_user_vaddr (path) || lookup_page(cur->pagedir, path, false) == NULL)
				quit(f);

		  struct dir *dir = get_dir_from_path(path);
		  
		  if(strcmp(path, "/")==0)
		  {
		  	cur->cur_dir = ROOT_DIR_SECTOR;
		  	f->eax = 1;
		  	return;
		  }
		  
		  if(dir == NULL)
  			f->eax = 0;
  		else
  		{	
  			char *cur_dir = get_name_from_path(path);
  			struct inode *inode_ptr;
  			if(dir_lookup(dir, cur_dir, &inode_ptr))
  			{
  				cur->cur_dir = inode_get_inumber( inode_ptr );
  				f->eax = 1;
  			}
  			else 
  			{
  				f->eax = 0;
  				dir_close(dir);
  				return;
  			}	
  		}	
  		dir_close(dir);
  		break;
		}
		case(SYS_MKDIR):
		{
			if(!check(1, f))
				quit(f);
			char *path = * (char **) (f->esp + 4);			
			if(path == NULL || !is_user_vaddr (path) || lookup_page(cur->pagedir, path, false) == NULL)
				quit(f);
			if(strcmp(path, "") == 0)
			{
				f->eax = 0;
			}
			else
			{
				
		 	  struct dir *dir = get_dir_from_path(path);
		 	 	if(dir == NULL)
  				f->eax = 0;
  			else
  			{
  				disk_sector_t new_dir_inode;
  				free_map_allocate(1, &new_dir_inode);
  				
  				char *dir_name = get_name_from_path(path);
					char n[NAME_MAX + 1];
					
					while(dir_readdir(dir, n))
					{
						if(strcmp(dir_name, n) == 0)
						{
							f->eax = 0;
							return;
						}	
					}

  				f->eax = dir_create(new_dir_inode, inode_get_inumber(dir_get_inode(dir)), 16, dir_name);
  				
  			}  			
  			dir_close(dir);
  		}
  		
  		break;
		}
		case(SYS_READDIR):
		{
	
			if(!check(2, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);	
			char *name = * (char **) (f->esp + 8);
			if(name == NULL || !is_user_vaddr (name) || lookup_page(cur->pagedir, name, false) == NULL)
				quit(f);
				
			
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd!=NULL)
			{		
				struct dir *dir = cur_fd->dir;
				f->eax = dir_readdir(dir, name);
			}	
			break;
		}
		case(SYS_ISDIR):
		{
	
			if(!check(1, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);	
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd!=NULL)
				f->eax = cur_fd->is_dir;
			break;
		}
		case(SYS_INUMBER):
		{
			if(!check(1, f))
				quit(f);
			int fd_no = * (int *) (f->esp + 4);	
			struct fd *cur_fd = get_fd_by_no(fd_no);
			if(cur_fd!=NULL)
				f->eax = cur_fd->dir->inode->sector;
			
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

