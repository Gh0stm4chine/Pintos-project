#include "threads/palloc.h"

static struct list frame_table;

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


