#include "threads/palloc.h"

struct frame_table
{
  struct lock lock;
  struct bitmap *used_map;
  uint8_t *base;
};

void falloc_init(size_t frame_limit){}

static void *falloc_get_frame(enum palloc_flags flags){
  return palloc_get_page(flags);
}

static void falloc_free_frame(void *page){
  palloc_free_page(page);
}

static void 

// addr = palloc_get_page(PAL_USER);
// if(addr == NULL)
//   switchPage();


