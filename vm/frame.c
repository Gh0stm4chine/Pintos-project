#include "threads/palloc.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/frame.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/frame.h"

struct
{
  struct lock lock;
  int size;
  struct frame** frames;
} frame_table;


void frames_init(size_t frame_limit){
  frame_table.frames = malloc(sizeof(struct frame *)*frame_limit);
  frame_table.size = frame_limit;
  int i;
  for(i=0; i<frame_limit; i++){
    frame_table.frames[i] = malloc(sizeof(struct frame));
    frame_table.frames[i]->free = 1;
  }
}

void* frame_evict(void){
  struct frame f = *frame_table.frames[0];
  printf("evict a frame, %d, %x, %x, %x, %d, %d \n", f.tid, f.pd, f.upage, f.kpage, f.writable, f.free);
  thread_exit();
}

void frame_set(void *page){
  int i;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i]->free == 1){
      frame_table.frames[i]->kpage = page;
      frame_table.frames[i]->upage = NULL;
      frame_table.frames[i]->free = 0;
    }
  }
}

void frame_update(int tid, uint32_t *pd, void *upage, void *kpage, int writable){
  int i = 0;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i]->kpage == kpage){
      *frame_table.frames[i] = (struct frame){tid, pd, upage, kpage, writable, free};
    }
  }
}

void frame_free(void *page){
  int i = 0;
  for(i=0; i<frame_table.size; i++){
    if(frame_table.frames[i]->kpage == page){
      frame_table.frames[i]->free = 1;
      break;
    }
  }
}


