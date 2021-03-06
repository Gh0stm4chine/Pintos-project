#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_DIRECT 115
#define NUM_SINDIR 5
#define NUM_DINDIR 128-3-NUM_DIRECT-NUM_SINDIR

/* Max size of an inode's data */
#define INODE_CAPACITY NUM_DIRECT*BLOCK_SECTOR_SIZE+NUM_SINDIR*128*BLOCK_SECTOR_SIZE+NUM_DINDIR*128*128*BLOCK_SECTOR_SIZE

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t data[NUM_DIRECT+NUM_SINDIR+NUM_DINDIR];
  };

struct data_block
  {
    uint32_t data[128];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {    
    int length;
    int sectors;
    int sectors1;
    int sectors21;
    int sectors22;
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    //struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  struct inode_disk *disk_inode = malloc(sizeof *disk_inode);
  cache_read (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
  block_sector_t sector;
  ASSERT (inode != NULL);
  if (pos < disk_inode->length)
    if (pos < NUM_DIRECT*BLOCK_SECTOR_SIZE)
      sector = disk_inode->data[pos/BLOCK_SECTOR_SIZE];
    else
      if (pos < NUM_DIRECT*512 + NUM_SINDIR*128*512){
	int a = (pos-NUM_DIRECT*512)/(128*512);
	struct data_block *idata = malloc(sizeof *idata);
	cache_read (fs_device, idata, disk_inode->data[NUM_DIRECT+a], 0, BLOCK_SECTOR_SIZE);
	pos -=  NUM_DIRECT*512 + a*128*512;
	sector = idata->data[pos/BLOCK_SECTOR_SIZE];
	free(idata);
      }else{
	int a = (pos-NUM_DIRECT*512-NUM_SINDIR*128*512)/(128*128*512);
	struct data_block *idata = malloc(sizeof *idata);
	cache_read (fs_device, idata, disk_inode->data[NUM_DIRECT+NUM_SINDIR+a], 0, BLOCK_SECTOR_SIZE);
	pos -= NUM_DIRECT*512 + NUM_SINDIR*128*512 + a*128*128*512;
	int b = pos/(128*512);
	struct data_block *i2data = malloc(sizeof *idata);
	cache_read (fs_device, i2data, idata->data[b], 0, BLOCK_SECTOR_SIZE);
	free(idata);
	pos -= b*128*512;
	sector = i2data->data[pos/BLOCK_SECTOR_SIZE];
	free(i2data);
      }
    //return disk_inode->start + pos / BLOCK_SECTOR_SIZE;
  else
    sector = -1;
  free(disk_inode);
  //printf("sector %u %d\n", sector, pos);
  return sector;

  /* struct inode_disk *disk_inode = malloc(sizeof *disk_inode);
  cache_read (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
  ASSERT (inode != NULL);
  block_sector_t sector;
  if (pos < disk_inode->length){
    sector = disk_inode->start + pos / BLOCK_SECTOR_SIZE;
  }
  else{
    sector = -1;
  }
  free(disk_inode);
  return sector;*/
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  cache_init();
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  //printf("inode create\n");
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          cache_write (fs_device, disk_inode, sector, 0, BLOCK_SECTOR_SIZE);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++){ 
                cache_write (fs_device, zeros, disk_inode->start + i, 0, BLOCK_SECTOR_SIZE);
		if (i < NUM_DIRECT)
		  disk_inode->data[i] = disk_inode->start + i;
		else 
		  if (i < NUM_DIRECT + NUM_SINDIR*128){
		    int a = (i-NUM_DIRECT)/128;
		    if((i-NUM_DIRECT)%128 == 0){
		      ASSERT(free_map_allocate (1, &disk_inode->data[NUM_DIRECT+a]));
		      //printf("creating indirect at %d",NUM_DIRECT+a);
		    }
		    struct data_block *idata = malloc(sizeof *idata);
		    cache_read (fs_device, idata, disk_inode->data[NUM_DIRECT+a], 0, BLOCK_SECTOR_SIZE);
		    idata->data[i-NUM_DIRECT-a*128] = disk_inode->start + i;
		    //printf("writing indirect data %d, %d, %d",NUM_DIRECT+a, i-NUM_DIRECT-a*128, disk_inode->start + i);
		    cache_write (fs_device, idata, disk_inode->data[NUM_DIRECT+a], 0, BLOCK_SECTOR_SIZE);
		    free(idata);
		  }
		  else{
		    int a = (i-NUM_DIRECT-NUM_SINDIR*128)/(128*128);
		    struct data_block *idata = malloc(sizeof *idata);
		    if((i-NUM_DIRECT-NUM_SINDIR*128)%(128*128) == 0)
		      ASSERT(free_map_allocate (1, &idata->data[a]));
		    cache_read (fs_device, idata, disk_inode->data[a], 0, BLOCK_SECTOR_SIZE);
		    int y = NUM_DIRECT+NUM_SINDIR*128+a*128*128;
		    int b = (i-y)/128;
		    struct data_block *i2data = malloc(sizeof *idata);
		    cache_read (fs_device, i2data, idata->data[a], 0, BLOCK_SECTOR_SIZE);
		    i2data->data[i-NUM_DIRECT-NUM_SINDIR*128-a*128*128-b*128] = disk_inode->start + i;
		  }		    
		   		    
	      }	    
	      cache_write (fs_device, disk_inode, sector, 0, BLOCK_SECTOR_SIZE);
	      
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  struct inode_disk *disk_inode = malloc(sizeof *disk_inode);
  cache_read (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
  inode->length = disk_inode->length;
  inode->sectors = 0;
  inode->sectors1 = 0;
  inode->sectors21 = 0;
  inode->sectors22 = 0;
  if(inode->length < NUM_DIRECT){
    inode->sectors = inode->length/BLOCK_SECTOR_SIZE;
  }else if(inode->length < NUM_DIRECT + NUM_SINDIR*128){
    inode->sectors = NUM_DIRECT;
    inode->sectors1 = (inode->length-NUM_DIRECT*512)/(128*512);
  } else {
    inode->sectors = NUM_DIRECT;
    inode->sectors1 = NUM_SINDIR;
    inode->sectors21 = (inode->length-NUM_DIRECT*512-NUM_SINDIR*128*512)/(128*128*512);
    inode->sectors22 = (inode->length-NUM_DIRECT+NUM_SINDIR*128+inode->sectors21*128*128*512)/(128*128*512);
  }
  
  free(disk_inode);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
	  struct inode_disk *disk_inode = malloc(sizeof *disk_inode);
	  cache_read (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
	  int sectors = inode->length/BLOCK_SECTOR_SIZE;
	  while(inode->length >= 0){
	    free_map_release (byte_to_sector(inode, inode->length),1);
	    inode->length -= BLOCK_SECTOR_SIZE;
	  }
	  if(sectors > NUM_DIRECT){
	    sectors -= NUM_DIRECT;
	    if(sectors > NUM_SINDIR*128){
	      int sectors2 = sectors-NUM_SINDIR*128;
	      while(sectors2/128 >= 0){
		struct data_block *idata = malloc(sizeof *idata);
		cache_read (fs_device, idata, disk_inode->data[sectors2/(128*128)+NUM_DIRECT+NUM_SINDIR*128], 0, BLOCK_SECTOR_SIZE);
		int a = sectors2/(128*128);
		int i = 0;
		for(i=0; i < sectors2/128 - a*128; i++)
		  free_map_release (idata->data[i],1);
		free_map_release (disk_inode->data[a+NUM_DIRECT+NUM_SINDIR*128],1);
		sectors2 -= 128;
	      }
	    }
	    sectors = sectors/128;
	    while(sectors >= 0){
	      free_map_release (disk_inode->data[NUM_DIRECT + sectors],1);
	      sectors -= 1;
	    }
	  }
	      

          free_map_release (inode->sector, 1);
	  free(disk_inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  //uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      
      cache_read(fs_device, buffer+bytes_read, sector_idx, sector_ofs, chunk_size);
      /*
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
	/* Read full sector directly into caller's buffer. *//*
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
	  into caller's buffer. *//*
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }*/
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  //uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;
  //printf("inode start write %d\n",size);
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      off_t inode_left = inode_length (inode) - offset;
      if(sector_idx == -1 || inode_left < size){
	//printf("inode size %d, sector %d\n", inode_length(inode), sector_idx);
	sector_idx = inode_grow(inode, size, sector_left);
	inode_left = inode_length(inode) - offset;
	//printf("inode resize %d, sector %d\n", inode_length(inode), sector_idx);
      }
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      //printf("chunk_size %d, %d, %d, %d, %d, %d ,%d\n",sector_idx, sector_ofs, inode_left, offset, sector_left, min_left, chunk_size);
      if (chunk_size <= 0)
        break;
      
      cache_write(fs_device, buffer + bytes_written, sector_idx, sector_ofs, chunk_size);

      /*
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
	/* Write full sector directly to disk. *//*
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
	/* We need a bounce buffer. *//*
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. *//*
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }*/

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  //free (bounce);

  //printf("inode write %d, %d \n", bytes_written, inode_length(inode));
  return bytes_written;
}

block_sector_t 
inode_grow(struct inode *inode, int size, int sector_left)
{  
  struct inode_disk *disk_inode = malloc(sizeof *disk_inode);
  cache_read (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
  ASSERT (inode != NULL);
  int pos = disk_inode->length;
  int new = pos + size;
  int sector;
  if (size < sector_left && pos != 0){
    disk_inode->length += size;
    sector = disk_inode->data[pos/BLOCK_SECTOR_SIZE];
    cache_write (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
  } else {
    if (new < NUM_DIRECT*BLOCK_SECTOR_SIZE){
      // printf("%d, %d \n",pos,pos/BLOCK_SECTOR_SIZE); 
      if (free_map_allocate(1, &disk_inode->data[new/BLOCK_SECTOR_SIZE])) 
        {
	  sector = disk_inode->data[pos/BLOCK_SECTOR_SIZE];
	  disk_inode->length += size;
	  // printf("growing directly %d\n", disk_inode->length);
          cache_write (fs_device, disk_inode, inode->sector, 0, BLOCK_SECTOR_SIZE);
	}
    }
    else{
      if (pos < NUM_DIRECT*512 + NUM_SINDIR*128*512){
	int a = (pos-NUM_DIRECT*512)/(128*512);
	struct data_block *idata = malloc(sizeof *idata);
	cache_read (fs_device, idata, disk_inode->data[NUM_DIRECT+a], 0, BLOCK_SECTOR_SIZE);
	pos -=  NUM_DIRECT*512 + a*128*512;
	sector = idata->data[pos/BLOCK_SECTOR_SIZE];
	free(idata);
      }else{
	int a = (pos-NUM_DIRECT*512-NUM_SINDIR*128*512)/(128*128*512);
	struct data_block *idata = malloc(sizeof *idata);
	cache_read (fs_device, idata, disk_inode->data[NUM_DIRECT+NUM_SINDIR+a], 0, BLOCK_SECTOR_SIZE);
	pos -= NUM_DIRECT*512 + NUM_SINDIR*128*512 + a*128*128*512;
	int b = pos/(128*512);
	struct data_block *i2data = malloc(sizeof *idata);
	cache_read (fs_device, i2data, idata->data[b], 0, BLOCK_SECTOR_SIZE);
	free(idata);
	pos -= b*128*512;
	sector = i2data->data[pos/BLOCK_SECTOR_SIZE];
	free(i2data);
      }
    }
  }
  inode->length = disk_inode->length;
  // printf("inode grow %d %d \n",sector, size);
  free(disk_inode);
  return sector;
}


/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}
