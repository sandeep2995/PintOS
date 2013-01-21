#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char **cmdline, void (**eip) (void), void **esp);



/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
	
  char *fn_copy;
  tid_t tid;
	struct thread *child;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
	
	// taking the first token of the string as the name //
	int i;
	char *name = (char *)malloc(sizeof(char));
  for(i=0;i<strlen(file_name);i++)
  {
  	if(file_name[i]==' ')
  		break;
  	name[i] = file_name[i];
  }
  name[i] = '\0';
	//-----added code ends -----//
	
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (name, PRI_DEFAULT, start_process, fn_copy);
 	
 	free(name);
  
  /*
  	The thread is the parent thread creating a child thread, 
  	it is blocked so that the parent cannot execute until the 
  	child loads successfully  
  */
 	/**********added for 2B: inserting into child list*******/
 	//child = get_thread_by_id(tid);
 	//list_push_front(&thread_current()->child_list, &child->child_list_elem);
 	//----------added code ends---------------//
  
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
	
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

	/************2D************************************/
	lock_acquire(&thread_current()->parent->child_lock);

	/****************************************************
	Modifying code for project 2A
	****************************************************/
	char *token, *save_ptr;
	char **args;
	int i=0, count=0;
	args = (char **)malloc(sizeof(char *));
	for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr))
	{
		*args = (char *)malloc(sizeof(char));
		*args = token;
	
		args++;
		i++;
		count++;
	}	
	*args = NULL;
	while(i>0)
	{
		args--;
		i--;
	}
	//----added code ends------//



  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (args, &if_.eip, &if_.esp);

	if(!success)
	{
		thread_current()->parent->child_load_failed = true;
	}
	lock_release(&thread_current()->parent->child_lock);



  /* If load failed, quit. */
  palloc_free_page (args[0]);
  if (!success) 
    thread_exit ();
 
    
	

	
	/***********Freeing the memory allocated****************
	for(i=0;i<count-1;i++)
	{printf("here\n");
		free(args[i]);
	}
	free(args);
	/*******************************************************/


  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");

  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
	struct thread *t = get_thread_by_id(child_tid);
	struct thread *cur = thread_current();
	int ret_val;
	
	struct list_elem *e;
	if(t==NULL)
	{
		for(e=list_begin(&cur->dead_child_list);e!=list_end(&cur->dead_child_list);e=list_next(e))
		{
			struct dead_thread *cur_dead_thread = list_entry(e, struct dead_thread, elem);
			if(cur_dead_thread->tid == child_tid)
			{
				ret_val = cur_dead_thread->exit_status;
				list_remove(e);
				free(cur_dead_thread);	
				return ret_val;
			}	
		}
		return -1;
	} 
	
	
	for(e=list_begin(&cur->child_list);e!=list_end(&cur->child_list);e-list_next(e))
	{
		if(list_entry(e, struct thread, child_list_elem)->tid == child_tid)
		 break;	
	}	 
	if(e==list_end(&cur->child_list))
		return -1;
	else if(t->waited_on_by_parent == true)
		return -1; 
	
	thread_current()->waiting_on_child = t;
	t->waited_on_by_parent = true;

	thread_block(); 
	for(e=list_begin(&cur->dead_child_list);e!=list_end(&cur->dead_child_list);e=list_next(e))
	{
		struct dead_thread *cur_dead_thread = list_entry(e, struct dead_thread, elem);
		if(cur_dead_thread->tid == child_tid)
		{
			ret_val = cur_dead_thread->exit_status;
			list_remove(e);
			free(cur_dead_thread);	
			return ret_val;
		}
	}
	//-----added code ends-----//

}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
	int i;
	
	for(i=0;i<1024;i++)
	{
		if(frame_table[i].tid == cur->tid)
			frame_table[i].occupied = false;
	}
	struct spt *page;
	while(!list_empty(&thread_current()->sup_pt))
	{
		page = list_entry(list_pop_front(&thread_current()->sup_pt), struct spt, elem);
		if(page->type == 3)
		{
			swap_slot_occupied[page->swap_slot] = false;
		}
		free(page);
	}	
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
    	
    	
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char **file_args);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
bool load_segment (struct file *file, int32_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);


/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char **file_args, void (**eip) (void), void **esp) 
{

  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  bool all_zero = false;										//added for project 3
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_args[0]);
  
   
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_args[0]);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_args[0]);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                  
                  all_zero = true;
                }
                
              /*************Fill the supplementary page table**************/
						  fill_spt(file, file_page, (void *) mem_page, read_bytes, zero_bytes, writable);
						  /************************************************************/
						  
              //if (!load_segment (file, file_page, (void *) mem_page,
              //                   read_bytes, zero_bytes, writable))
              //  goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */

  if (!setup_stack (esp, file_args))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
	
 done:
  /* We arrive here whether the load is successful or not. */
	
	//file_close(file);
 
  
	return success;
}

/* load() helpers. */

bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
	file_open (file);
	file_seek (file, ofs);
  

  
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
      {
        /*****Implement the page eviction strategy here***********/
		kpage = evict_and_allot();
			
      }  
			
      /* Load this page. */
      int no_of_bytes_read = file_read (file, kpage, page_read_bytes);
      if (no_of_bytes_read != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);
		
      /* Add the page to the process's address space. */
     
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }
			
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char **file_args) 
{

  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if(kpage == NULL)
	{
		kpage = evict_and_allot();
		
	}
	
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        //*esp = PHYS_BASE;
       	/*****************************************************************
       	Modifying code to set up stack for project 2A
       	*****************************************************************/ 

       	*esp = PHYS_BASE;
//       	while(* (int *) esp & 3 != 0)
 //      	{
 //     		*esp = *esp - 1;
 //     	}
       	int i, count = 0;
       	for(i=0, count=0;file_args[i]!=NULL;i++, count++)
       	{}
       	char *add[count];
       	for(i=count-1;i>=0;i--)
       	{
       		*esp = *esp - strlen(file_args[i]) - 1;
       		add[count-i-1] = *esp;
       		strlcpy(*esp, file_args[i], strlen(file_args[i])+1);
       		//printf("\nSTACK POINTER %p: %s\n", *esp, * (char **) esp);
       	}
       	while(* (int *) esp & 3 != 0)
       	{
      		*esp = *esp - 1;
      		** (uint8_t **) esp = (uint8_t)0;
      	}
       	*esp-=4;
  			** (char ***) esp = NULL;     	
       	for(i=0;i<count;i++)
       	{
       		*esp = *esp-4;
       		** (char ***) esp = add[i];
       		//printf("\nSTACK POINTER %p  %p  %s\n", *esp, ** (char ***) esp, ** (char ***) esp); 
       	}
       	char **zero_add;
       	zero_add = *esp;
       	*esp-=4;
       	** (char ****) esp = zero_add;
       	//printf("\nSTACK POINTER %p  %p.\n",*esp,**(char***)esp); 
       	*esp -= sizeof(int);
       	** (int **) esp = count;
       	//printf("\nSTACK POINTER:%p  %d.\n",*esp,**(int**)esp);
       	*esp -= sizeof(void *);
       	** (int **) esp = 0;
       	//------added code ends---------------------//
      }  
      else
        palloc_free_page (kpage);

	
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

	bool ret_val;
	struct thread *cur = thread_current();
	tid_t thread_id = thread_current()->tid;
	 /* Verify that there's not already a page at that virtual
     address, then map our page there. */
     
	 

	ret_val = (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable)); 
          
          
	/*****************Project 3B: Supplementary Page Table*******************/
	
	if(ret_val)
	{
		
		uint32_t frame_no = (vtop(kpage) & 0x00FFF000) >> 12;
		frame_table[frame_no].v_addr = upage;
		frame_table[frame_no].tid = thread_id;
		frame_table[frame_no].occupied = true;		
		frame_table[frame_no].kpage = kpage;
		frame_table[frame_no].writable = writable;	
		set_spt(cur, upage, 0, NULL, -1, -1, writable);
	}	
	/************************************************************************/

  return (ret_val);
}
/**************Adding function for project 3ABC******************************/
void fill_spt(struct file *file, int32_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
	
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

   
			struct spt *cur_page = (struct spt *)malloc(sizeof(struct spt));
			cur_page->v_addr = upage;
			
			cur_page->file = file;
			cur_page->ofs = ofs;
			cur_page->read_bytes = read_bytes;
			cur_page->zero_bytes = zero_bytes;
			cur_page->writable = writable;
			list_push_back(&thread_current()->sup_pt, &cur_page->elem);
			if(page_read_bytes!=0)
				cur_page->type = 1;	
			else 	
				cur_page->type = 2;
			
			
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }
}
void set_spt(struct thread *t, uint8_t *upage, int type, struct file *file, int32_t ofs, int swap_slot, bool writable)
{
	struct list_elem *e;

	for(e=list_begin(&t->sup_pt); e!=list_end(&t->sup_pt); e=list_next(e))
	{
		struct spt *page = list_entry(e, struct spt, elem);
		if(page->v_addr == upage)
		{
			page->type = type;
			page->file = file;
			page->ofs = ofs;
			page->swap_slot = swap_slot;
			page->writable = writable;
			break;
		}
	}
	if(e==list_end(&t->sup_pt))
	{
			struct spt *cur_page = (struct spt *)malloc(sizeof(struct spt));
			cur_page->v_addr = upage;
			cur_page->type = type;
			cur_page->file = file;
			cur_page->ofs = ofs;
			cur_page->writable = writable;
			cur_page->swap_slot = swap_slot;
			list_push_back(&t->sup_pt, &cur_page->elem);
		
	}
}
/***************************************************************************/
/**************Adding function for project 3D******************************/
uint8_t * evict_and_allot()
{

	int i;
	for(i=(last_evicted_frame+1)%1024;;i=(i+1)%1024)
	{
		i = i>653?i:654;
		uint32_t * v_addr = frame_table[i].v_addr;
		struct thread *t = get_thread_by_id(frame_table[i].tid);
		
		bool b = pagedir_is_accessed(t->pagedir, v_addr);
		
		if(b)
		{
			pagedir_set_accessed(t->pagedir, v_addr, false);
		}
		else
		{
			//victim found
			void * kernel_virtual_addr = frame_table[i].kpage;
			
			struct disk *swap_disk = disk_get(1, 1); 
			int j, slot_no;
			for(j=0;j<1024;j++)
			{
				if(!swap_slot_occupied[j])
				{
					slot_no = j;
					break;
				}	
			}
			
			swap_slot_occupied[slot_no] = true;
			
			for(j=0;j<8;j++)
			{
				disk_write(swap_disk, slot_no*8+j, (uint32_t *)((uint32_t)kernel_virtual_addr+(j*512)));	
			}
	
			set_spt(t, v_addr, 3, NULL, -1, slot_no, frame_table[i].writable);
			last_evicted_frame = i;
			pagedir_clear_page(t->pagedir, v_addr);
			
			break;
		}
	}
	return frame_table[i].kpage;
}
/***************************************************************************/
