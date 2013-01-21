#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */

    struct list_elem allelem;           /* List element for all threads list. */
	  int64_t ticks_to_sleep;							/* to store the ticks to sleep init to -1 */ 
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    
		struct lock *waiting_on_lock;				/* To store the lock on which the thread is waiting */
		struct lock *donated_for_lock;			/* To store the lock for which the thread has been conated last priority */
		struct list pri_list;								/* To store the priorities in a list */
		
		struct thread *parent;							/* To keep track of parent thread that created the thread */
		struct list child_list;							/* Each thread has child_list to store the children it created */
		struct list_elem child_list_elem;		/* To insert thread into child_list */
		struct thread *waiting_on_child;		/* For multiple children, to store the child on which thread is waiting */
		
		struct list fd_list; 								/* File Descriptor List */
		struct lock child_lock;							/* Lock for the parent to wait on child */
		bool child_load_failed;							/* Boolean to store if the child load failed */
		struct list dead_child_list;				/* Store a list of dead children of a process */
		bool waited_on_by_parent;						/* Did parent thread already wait on this thread */
		
		struct list sup_pt;									/* Supplementary page table per process */
		
		int no_of_stack_pages;						/* Keeps track of the no of stack pages the user process has */
		

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };
  
  /*******************************************************************/
  struct pri
  {
  	int priority;
  	struct lock *lock;
  	struct list_elem elem;
  };
  /*******************************************************************/
  
  /***********Added for 2D********************************************/
  struct fd
  {
  	int file_no;
  	struct file *file;
  	struct list_elem elem;
  };
  struct dead_thread
  {
  	int tid;
  	int exit_status;
  	struct list_elem elem;
  };
  /*******************************************************************/
  
  /********************Added for project 3******************************/
  struct fte
  {
  	uint32_t v_addr;
  	uint8_t *kpage;
  	bool writable;
  	tid_t tid;
  	bool occupied;
  };
  struct fte frame_table[1024];
  struct spt
  {
  	uint32_t v_addr;
  	int type;						/* 0:in memory, 1:in file, 2:all zeroes, 3:in swap area */
  	bool writable;
  	struct file *file;
  	int32_t ofs;						
  	uint32_t read_bytes;
  	uint32_t zero_bytes;
  	int swap_slot;			/////		/*May need to change to some other data type later on*/
  	struct list_elem elem;
  };
  /*********************************************************************/
  
  
/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);


struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* list to store the sleeping threads *******************************************/
struct list sleep_list; 
//***********************************************************************/
/***************added for project 3D page eviction********************/
int last_evicted_frame;
bool swap_slot_occupied[1024];
/*********************************************************************/
#endif /* threads/thread.h */
