#include "threads/palloc.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/block.h"

struct{
  struct block* block;
  int size;
  struct lock lock;
  struct swap* swaps;
}swap_table;

void swap_init(){
  swap_table.block = block_get_role(BLOCK_SWAP);
  swap_table.size = (int)block_size(swap_table.block);
  swap_table.swaps = malloc(sizeof(struct swap)*swap_table.size/8);
  int i;
  for(i=0; i<swap_table.size; i++)
    swap_table.swaps[i].free = 1;
  lock_init(&swap_table.lock);
}

void swap_add(struct frame* frame){
  int i, n;
  for(i=0; i<swap_table.size; i++){
    if(swap_table.swaps[i].free){
      swap_table.swaps[i].tid = frame->thread->tid;
      swap_table.swaps[i].upage = frame->upage;
      for(n=0; n<8; n++){
	block_write(swap_table.block, i*8+n, frame->kpage+n*BLOCK_SECTOR_SIZE);
      }
      swap_table.swaps[i].free = 0;
      //printf("swapping out %d, %x, %x, %d\n", frame->thread->tid, frame->upage, frame->kpage, i);
      return;
    }
  }
  printf("failing swap\n");
  thread_exit();
}

int swap_get(void* fault_adder, struct thread *t){
  if(lock_held_by_current_thread (&swap_table.lock))
    printf("already locked swap\n");
  lock_acquire(&swap_table.lock);
  int i, n;
  for(i=0; i<swap_table.size; i++){
    if(t->tid == swap_table.swaps[i].tid && pg_round_down(swap_table.swaps[i].upage) == pg_round_down(fault_adder) && !swap_table.swaps[i].free){
      void *kpage = palloc_get_page(PAL_USER);
      for(n=0; n<8; n++){
	block_read(swap_table.block, i*8+n, kpage+n*BLOCK_SECTOR_SIZE);
      }
      pagedir_set_page(t->pagedir, pg_round_down(fault_adder), kpage, 1);
      invalidate_pagedir(t->pagedir);
      swap_table.swaps[i].free = 1;
      //printf("swapping  in %d, %x, %x, %d\n", t->tid, pg_round_down(fault_adder), kpage, i);
      lock_release(&swap_table.lock);
      return 1;
    }
  }
  lock_release(&swap_table.lock);
  return 0;
}

void swap_free(int tid, void *kpage){
  int i;
  for(i=0; i<swap_table.size; i++){
    if(tid == swap_table.swaps[i].tid && pg_round_down(swap_table.swaps[i].upage) == pg_round_down(kpage) && !swap_table.swaps[i].free){
      swap_table.swaps[i].free = 1;
      break;
    }
  }
}

void swap_free_tid(int tid){
  int i;
  for(i=0; i<swap_table.size; i++){
    if(tid == swap_table.swaps[i].tid && !swap_table.swaps[i].free){
      swap_table.swaps[i].free = 1;
    }
  }
}
