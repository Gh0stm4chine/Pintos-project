#ifndef SWAP_H
#define SWAP_H
#include "devices/block.h"

struct swap{
  int tid;
  void *upage;
  int free;
  block_sector_t sector;
};

void swap_int();
void swap_add(struct frame* frame); 
int swap_get(void* upage, struct thread *t);
void swap_free(int tid, void *kpage);
void swap_free_tid(int tid);

#endif
