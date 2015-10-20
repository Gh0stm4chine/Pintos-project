#ifndef FRAME_H
#define FRAME_H


struct frame {
  int tid;
  uint32_t *pd;
  void *upage;
  void *kpage;
  int writable;
  int free;
};

void frames_init(size_t frame_limit);
void* frame_evict(void);
void frame_set(void *page);
void frame_free(void *page);

#endif
