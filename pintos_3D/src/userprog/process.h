#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

void fill_spt(struct file *file, int32_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void set_spt(struct thread *t, uint8_t *upage, int type, struct file *file, int32_t ofs, int swap_slot, bool writable);
uint8_t * evict_and_allot();
#endif /* userprog/process.h */
