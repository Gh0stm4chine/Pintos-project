
struct cache_element
{
  uint8_t data[BLOCK_SECTOR_SIZE];
  block_sector_t sector;
  int accessed;
  int dirty;
};

void cache_init (void);
void cache_flush (void);
void cache_write (struct block *fs_device, void *buffer, block_sector_t sector_idx, int sector_ofs, int chunk_size);
void cache_read (struct block *fs_device, void *buffer, block_sector_t sector, int sector_ofs, int chunk_size);
off_t cache_get_cache (struct inode *inode, void *buffer_, off_t size, off_t offset);
