#include "threads/palloc.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

struct{
  struct lock lock;
  int size;
  int clock;
  struct frame* frames;
} frame_table;


void frames_init(size_t frame_limit){
  frame_table.frames = malloc(sizeof(struct frame)*frame_limit);
  frame_table.size = frame_limit;
  frame_table.clock = 0;
  int i;
  for(i=0; i<frame_limit; i++){
    frame_table.frames[i].free = 1;
  }
  lock_init(&frame_table.lock);
}

void* frame_evict(void){
  if(lock_held_by_current_thread (&frame_table.lock))
    printf("already locked swap\n");
  lock_acquire(&frame_table.lock);
  int i = frame_table.clock+1;
  int done = 0;
  struct thread* t = thread_current();
  struct frame f;

  //printf("clock start %d, %d\n", i, frame_table.clock);
  while(i != frame_table.clock){
    f = frame_table.frames[i];
    if(f.thread != NULL){
      //printf("checking %d, %d, %d\n",i,pagedir_is_accessed( f.thread->pagedir, f.upage), pagedir_is_dirty(f.thread->pagedir, f.upage));
      if(pagedir_is_accessed( f.thread->pagedir, f.upage) == 0 && pagedir_is_dirty(f.thread->pagedir, f.upage) == 0){
	done = 1;
	frame_table.clock = i;
	break;
      } 
      pagedir_set_accessed(f.thread->pagedir, f.upage, 0);   
    }
    i += 1;
    if(i == frame_table.size)
      i = 0; 
  }
  //printf("clock 2 %d, %d, %d\n", i, frame_table.clock, done);
  if(!done){
    i += 1;
    if(i == frame_table.size)
      i = 0;
    while(i != frame_table.clock){
      f = frame_table.frames[i];
      if(f.thread != NULL){
	if(pagedir_is_accessed(f.thread->pagedir, f.upage) == 0 && pagedir_is_dirty(f.thread->pagedir, f.upage) == 0){
	  frame_table.clock = i;
	  done = 1; 
	  break;
	}
	pagedir_set_dirty(f.thread->pagedir, f.upage, 0);
      } 
      i += 1;
      if(i == frame_table.size)
	i = 0;
    }
    //printf("clock 3 %d, %d, %d\n", i, frame_table.clock, done);
    if(!done){
      i += 1;
      if(i == frame_table.size)
	i = 0;
      while(f.thread == NULL){
	f = frame_table.frames[i];
	i += 1;
	if(i == frame_table.size)
	  i = 0;
      }
      frame_table.clock = i;
    }
    if(frame_table.clock == frame_table.size)
      frame_table.clock = 0;
  }
  
  if(f.thread == NULL)
    printf("error\n");
  //printf("evict frame, %d, %x, %x, %d, %d, %d | %d | %d\n", f.thread->tid, f.upage, f.kpage, f.writable, f.free, i, frame_table.clock, frame_table.size);
  if(f.writable)
    swap_add(&f);
  pagedir_clear_page(f.thread->pagedir, f.upage);
  f.thread == NULL;
  memset (f.kpage, 0, PGSIZE);
  lock_release(&frame_table.lock);
  return f.kpage;
  //thread_exit();
}

void frame_set(void *page){
  //lock_acquire(&frame_table.lock);
  int i;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i].free == 1){
      frame_table.frames[i].kpage = page;
      frame_table.frames[i].thread = NULL;
      frame_table.frames[i].free = 0;
      break;
    }
  }
  //lock_release(&frame_table.lock);
}


void frame_update(struct thread *t, void *upage, void *kpage, int writable){
  //lock_acquire(&frame_table.lock);
  int i = 0;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i].kpage == kpage){
      frame_table.frames[i] = (struct frame){t, upage, kpage, writable, 0};
      break;
    }
  }
  //lock_release(&frame_table.lock);
}

void frame_free(void *page){
  //lock_acquire(&frame_table.lock);
  int i = 0;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i].kpage == page && frame_table.frames[i].free){
      frame_table.frames[i].free = 1;
      break;
    }
  }
  swap_free(thread_current()->tid, page); 
  //printf("%d %x free in swap\n", thread_current()->tid, page);
  //lock_release(&frame_table.lock);
}

void frame_free_tid(int tid){
  int i = 0;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i].thread->tid == tid && frame_table.frames[i].free){
      frame_table.frames[i].free = 1;      
    }
  }
  swap_free_tid(tid); 
}    

