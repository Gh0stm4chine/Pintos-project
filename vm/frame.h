#ifndef FRAME_H
#define FRAME_H


struct frame {
  struct thread *thread;
  void *upage;
  void *kpage;
  int writable;
  int free;
};

void frames_init(size_t frame_limit);
void* frame_evict(void);
void frame_set(void *page);
void frame_update(struct thread *t, void *upage, void *kpage, int writable); 
void frame_free(void *page);
void frame_free_tid(int tid);

#endif
