#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

static struct cache_element cache[65];
static int clock;

void
cache_init()
{
  int n;
  for(n = 0; n < 65; n++){
    cache[n].sector = -1;
  }
  clock = 0;
}

int
cache_evict()
{
  int n = clock + 1;
  if(n == 65)
    n = 0;
  while(n != clock){
    if(!cache[n].accessed && !cache[n].dirty){
      clock = n;
      return n;
    }
    cache[n].accessed = 0;
    n += 1;
    if(n == 65)
      n = 0; 
  }

  n += 1;
  if(n == 65)
    n = 0;
  while(n != clock){
      if(!cache[n].accessed && !cache[n].dirty){
      clock = 0;
      return n;
    }
    cache[n].dirty = 0;
    n += 1;
    if(n == 65)
      n = 0; 
  }
  
  n += 1;
  if(n == 65)
    n = 0;
  clock = n;
  return n;
}

int
cache_find(block_sector_t sector, struct block *fs_device)
{
  //printf("finding sector %d ",sector);
  int n;
  for(n=0; n<65; n++){
    if(cache[n].sector == sector){
      //printf("found %d, %d\n",n,cache[n].sector);
      cache[n].sector = sector;
      return n;
    }
  }
  for(n=0; n<65; n++){
    if(cache[n].sector == -1){
      block_read (fs_device, sector, cache[n].data);
      cache[n].sector = sector;
      cache[n].dirty = 0;
      //printf("made %d\n",n);
      return n;
    }
  }
  n = cache_evict();
  block_write (fs_device, cache[n].sector, cache[n].data);
  block_read (fs_device, sector, cache[n].data);
  cache[n].sector = sector;
  cache[n].dirty = 0;
  //printf("replaced %d\n",n);
  return n;
}

void
cache_write (struct block *fs_device, void *buffer, block_sector_t sector_idx, int sector_ofs, int chunk_size)
{
  //printf("cache write %d, %d\n", sector_idx,  chunk_size);
  int n = cache_find(sector_idx, fs_device);
  memcpy (cache[n].data + sector_ofs, buffer, chunk_size);
  //hex_dump(cache[n].data + sector_ofs,cache[n].data + sector_ofs,16,0); 
  cache[n].dirty = 1;
  cache[n].accessed = 1;
}

void
cache_read (struct block *fs_device, void *buffer, block_sector_t sector, int sector_ofs, int chunk_size)
{
  //printf("cache_read %d, %d\n", sector, chunk_size);
  int n = cache_find(sector, fs_device);
  memcpy (buffer, cache[n].data + sector_ofs, chunk_size);
  //hex_dump(buffer,buffer,16,0);
  cache[n].accessed = 1;
}

void
cache_flush ()
{
  int n;
  for(n = 0; n < 65; n++){
    block_write (fs_device, cache[n].sector, cache[n].data);
  }
}


