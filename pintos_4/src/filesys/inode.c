#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44



/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}



/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  
  
  
  
  if (pos < inode->data.length)
  {/*return inode->data.start + pos / DISK_SECTOR_SIZE;
  		/*************Added for project 4********************/
  		int index = pos / DISK_SECTOR_SIZE;
  		if(index <= 12)	
		  		return inode->data.file_index[index];
		  else if(index>12 && index<=140)
		  {
		  		disk_sector_t read_file_index[DISK_SECTOR_SIZE / 4];
		  		disk_read(filesys_disk, inode->data.file_index[13], read_file_index);
		  		return read_file_index[index - 13];
		  }		
		  else
		  {
		  		disk_sector_t read_first_file_index[DISK_SECTOR_SIZE / 4];
		  		disk_sector_t read_file_index[DISK_SECTOR_SIZE / 4];
		  		disk_read(filesys_disk, inode->data.file_index[14], read_first_file_index);
		  		off_t relative_pos = index - 141;
		  		off_t relative_index = relative_pos / (DISK_SECTOR_SIZE / 4);
		  		disk_read(filesys_disk, read_first_file_index[relative_index], read_file_index);
		  		return read_file_index[relative_pos % (DISK_SECTOR_SIZE / 4)];
		  }
			//----------------added code ends-----------------------------/
		
  }  
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
      size_t sectors = bytes_to_sectors (length);
      //printf("no of sectors: %d\n", sectors);
      size_t cont_sectors = sectors<=13 ? sectors : 13;
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      //printf("\ncont_sectors: %d\n", cont_sectors);
      if (free_map_allocate (cont_sectors, &disk_inode->start))
      {
          
          if (cont_sectors > 0) 
          {
              static char zeros[DISK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < 13; i++)
              { 
              		if(i<cont_sectors)
             			{
              				disk_inode->file_index[i] = disk_inode->start + i;
              				disk_write (filesys_disk, disk_inode->start + i, zeros); 
              		}
              		else
              				disk_inode->file_index[i] = 0;			
              }  
          } 
          success = true;
         // int i;
        //  for(i=0;i<13;i++)
   			//	{
   			//			printf("%d:%d\n", i, disk_inode->file_index[i]);
   			//	} 
      } 
      /*********************Added for project 4**********************/
      if(cont_sectors != sectors)
      {
      		
      		size_t remaining_sectors = sectors - cont_sectors;
      		size_t one_sector_sectors = DISK_SECTOR_SIZE / 4;
      		size_t first_level_sectors = remaining_sectors<=one_sector_sectors ? remaining_sectors : one_sector_sectors;
      		//printf("remaining sector: %d\tfirst_level_sectors: %d\n", remaining_sectors, first_level_sectors);
   				disk_sector_t one_level_file_index[one_sector_sectors];
   				int i;
   				disk_sector_t first_indirection;
   				free_map_allocate(1, &first_indirection);
   				//printf("first_inderection number %d\n", first_indirection);
   				for(i=0;i<one_sector_sectors;i++)
   				{
   						if(i<first_level_sectors)
   						{
   								disk_sector_t new_sector;
   								//printf("start\t");
              		free_map_allocate(1, &new_sector);
              		//printf("\tend");
   								one_level_file_index[i] = new_sector;
   						}
   						else
   								one_level_file_index[i] = 0;	
   					//	printf("%d:%d\n", i, one_level_file_index[i]);			
   				}		
   				disk_write(filesys_disk, first_indirection, one_level_file_index);
   				//disk_read(filesys_disk, first_indirection, one_level_file_index);
   				//for(i=0;i<one_sector_sectors;i++)
   				//{
   				//		printf("%d:%d\n", i, one_level_file_index[i]);
   				//}
   				disk_inode->file_index[13] = first_indirection;
      		remaining_sectors = remaining_sectors - first_level_sectors;
      		if(remaining_sectors != 0)
      		{
      				//printf("still remaining %d", remaining_sectors);
      				disk_sector_t second_indirection;
      				free_map_allocate(1, &second_indirection);
      				disk_sector_t second_level_file_index[one_sector_sectors];
      				int iterator = 0;
     			 		while(remaining_sectors > 0)
      				{
      						size_t new_level_sectors = remaining_sectors<=one_sector_sectors ? remaining_sectors : one_sector_sectors;
      						disk_sector_t new_indirection;
      						disk_sector_t new_level_file_index[one_sector_sectors];
      						free_map_allocate(1, &new_indirection);
      						second_level_file_index[iterator] = new_indirection;
      						for(i=0;i<one_sector_sectors;i++)
      						{
      								if(i<new_level_sectors)
      								{
      										disk_sector_t new_sector;
				              		free_map_allocate(1, &new_sector);
   												new_level_file_index[i] = new_sector;
      								}
      								else
      										new_level_file_index[i] = 0;
      								
      						}
      						disk_write(filesys_disk, new_indirection, new_level_file_index);
      						
      						
      						/* Advance */   						
      						remaining_sectors -= new_level_sectors;
      						iterator++;
      				}
      				disk_write(filesys_disk, second_indirection, second_level_file_index);
      				disk_inode->file_index[14] = second_indirection;
      		}		//printf("exiting\n");
      }
      disk_write (filesys_disk, sector, disk_inode);
      //--------------added code ends-------------------------------------------------------//  
      //int j;
      //for(j=0;j<13;j++)
      //{
      //		printf("%d:%d\n", j, disk_inode->file_index[j]);
      //}
      //disk_sector_t table[128];
      //disk_read(filesys_disk, disk_inode->file_index[13], table);
      //for(j=0;j<128;j++)
      //{
      //			printf("%d:%d\n", 13+j, table[j]);
      //}
      free (disk_inode);
  }
  
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
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
  disk_read (filesys_disk, inode->sector, &inode->data);
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
disk_sector_t
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
          disk_read(filesys_disk, inode->sector, &inode->data);
          int i;
          bool completed = false;
          for(i=0;i<13;i++)
          {
          		if(inode->data.file_index[i] == 0)
          		{
          				completed = true;
          				break;
          		}		
          		else
          		{
          				free_map_release(inode->data.file_index[i], 1);
          		}		
          }
          if(!completed)
          {
          		disk_sector_t first_table[DISK_SECTOR_SIZE/4];
          		disk_read(filesys_disk, inode->data.file_index[13], first_table);
          		for(i=0;i<DISK_SECTOR_SIZE/4;i++)
          		{
          				if(first_table[i] == 0)
          				{
          						completed = true;
          						break;
          				}
          				else
          				{
          						free_map_release(first_table[i], 1);
          				}
          		}
          		if(!completed)
          		{
          				disk_sector_t one_table[DISK_SECTOR_SIZE/4];
          				disk_read(filesys_disk, inode->data.file_index[14],one_table);
          				int j;
          				for(i=0;i<DISK_SECTOR_SIZE/4 && !completed;i++)
          				{
          						disk_sector_t two_table[DISK_SECTOR_SIZE/4];
          						disk_read(filesys_disk, one_table[i], two_table);
          						for(j=0;j<DISK_SECTOR_SIZE/4;j++)
          						{
          								if(two_table[j] == 0)
          								{
          										completed = true;
          										break;
          								}
          								else
          								{
          										free_map_release(two_table[j], 1);
          								}
          						}
          				}
          		}
          }
          free_map_release (inode->sector, 1);
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
  uint8_t *bounce = NULL;
	
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          disk_read (filesys_disk, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
	
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
  uint8_t *bounce = NULL;
  uint8_t *empty_buffer = NULL;
	off_t total_size = size;
  if (inode->deny_write_cnt)
    return 0;

	
  while (size > 0) 
  {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);

      int sector_ofs = offset % DISK_SECTOR_SIZE;
			
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
		
      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
      {
          /* Write full sector directly to disk. */
  
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
      }
      else 
      {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
          {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
          }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
  
          disk_write (filesys_disk, sector_idx, bounce); 
      }
	
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
  }
  if(bytes_written != total_size)
  {
  		
  		off_t bytes_to_write = total_size - bytes_written;
  		bool sparse = false;
  			
  		if(offset>inode->data.length)
  		{
  				
  				int sparse_bytes = offset-inode->data.length;
  				
  				void *sparse_buffer = malloc(sparse_bytes);
  				memset(sparse_buffer, 0, sparse_bytes);
  				
  				
  				if(inode->data.length % DISK_SECTOR_SIZE != 0)
  				{
  						/* Complete sector was not written, first write the remaining sector */
  						int sector_written = inode->data.length % DISK_SECTOR_SIZE;
  						int sector_left = DISK_SECTOR_SIZE - sector_written;
  						int write_bytes = sector_left<=sparse_bytes ? sector_left : sparse_bytes;
  						disk_sector_t sector_no = byte_to_sector(inode, inode->data.length - 1);
							if (bounce == NULL) 
				      {
				          bounce = malloc (DISK_SECTOR_SIZE);
				      }
				      disk_read(filesys_disk, sector_no, bounce);
				      memcpy(bounce + sector_written, sparse_buffer, write_bytes);
				     	disk_write(filesys_disk, sector_no, bounce);
							inode->data.length += write_bytes;
				      sparse_bytes -= write_bytes;
				  }
					while(sparse_bytes > 0)
					{
							int write_bytes = DISK_SECTOR_SIZE<=sparse_bytes ? DISK_SECTOR_SIZE : sparse_bytes;
							if (bounce == NULL) 
				      {
				          bounce = malloc (DISK_SECTOR_SIZE);
				          if (bounce == NULL)
				            break;
				      }
				      memset(bounce, 0, DISK_SECTOR_SIZE);
				      memcpy(bounce, sparse_buffer, write_bytes);
							disk_sector_t new_sector;
							free_map_allocate(1, &new_sector);
							disk_write(filesys_disk, new_sector, bounce);
							int file_length = inode->data.length;
							int logical_file_sector = (file_length+2)/DISK_SECTOR_SIZE;
							if(logical_file_sector <= 12)
							{
									inode->data.file_index[logical_file_sector] = new_sector;
							}
							else if(logical_file_sector>=13 && logical_file_sector<=140)
							{
									if(inode->data.file_index[13] == 0)
									{
											disk_sector_t first_indirection_sector;
											free_map_allocate(1, &first_indirection_sector);
											if(empty_buffer == NULL)
											{
													empty_buffer = malloc(DISK_SECTOR_SIZE);
											}
											memset(empty_buffer, 0, DISK_SECTOR_SIZE);
				
											disk_write(filesys_disk, first_indirection_sector, empty_buffer);
											inode->data.file_index[13] = first_indirection_sector;
									}	
									disk_sector_t one_level_file_index[DISK_SECTOR_SIZE / 4];
									disk_read(filesys_disk, inode->data.file_index[13], one_level_file_index);
									one_level_file_index[logical_file_sector - 13] = new_sector;
				
									disk_write(filesys_disk, inode->data.file_index[13], one_level_file_index);
							}
							else
							{
									if(inode->data.file_index[14] == 0)
									{
											disk_sector_t second_indirection_sector;
											free_map_allocate(1, &second_indirection_sector);
											if(empty_buffer == NULL)
											{
													empty_buffer = malloc(DISK_SECTOR_SIZE);
											}
											memset(empty_buffer, 0, DISK_SECTOR_SIZE);
				
											disk_write(filesys_disk, second_indirection_sector, empty_buffer);
											inode->data.file_index[14] = second_indirection_sector;
									}
									disk_sector_t first_level_file_index[DISK_SECTOR_SIZE / 4];
									disk_read(filesys_disk, inode->data.file_index[14], first_level_file_index);
									int first_level_index = (logical_file_sector - 141) / (DISK_SECTOR_SIZE / 4);
									int second_level_index = (logical_file_sector - 141) % (DISK_SECTOR_SIZE / 4);
									if(first_level_file_index[first_level_index] == 0)
									{
											disk_sector_t first_indirection_sector;
											free_map_allocate(1, &first_indirection_sector);
											if(empty_buffer == NULL)
											{
													empty_buffer = malloc(DISK_SECTOR_SIZE);
											}
											memset(empty_buffer, 0, DISK_SECTOR_SIZE);
				
											disk_write(filesys_disk, first_indirection_sector, empty_buffer);
											first_level_file_index[first_level_index] = first_indirection_sector;
									}
									disk_sector_t second_level_file_index[DISK_SECTOR_SIZE / 4];
									disk_read(filesys_disk, first_level_file_index[first_level_index], second_level_file_index);
									second_level_file_index[second_level_index] = new_sector;
				
									disk_write(filesys_disk, first_level_file_index[first_level_index], second_level_file_index);
				
									disk_write(filesys_disk, inode->data.file_index[14], first_level_file_index);
							}
							/* Advance */

							sparse_bytes -= write_bytes;
							inode->data.length += write_bytes;
							
					}
					free(sparse_buffer);
  		}
  		
  		
  		if(inode->data.length % DISK_SECTOR_SIZE != 0)
  		{
  				/* Complete sector was not written, first write the remaining sector */
  			
  				int sector_written = inode->data.length % DISK_SECTOR_SIZE;
  				int sector_left = DISK_SECTOR_SIZE - sector_written;
  				int write_bytes = sector_left<=bytes_to_write ? sector_left : bytes_to_write;
  				disk_sector_t sector_no = byte_to_sector(inode, inode->data.length - 1);
  				if (bounce == NULL) 
          {
              bounce = malloc (DISK_SECTOR_SIZE);
          }
          disk_read(filesys_disk, sector_no, bounce);
          memcpy(bounce + sector_written, buffer + bytes_written, write_bytes);
         	disk_write(filesys_disk, sector_no, bounce);
					inode->data.length += write_bytes;
          bytes_to_write -= write_bytes;
          offset += write_bytes;
          bytes_written += write_bytes; 
  		}
  		
			while(bytes_to_write > 0)
			{
					int write_bytes = DISK_SECTOR_SIZE<=bytes_to_write ? DISK_SECTOR_SIZE : bytes_to_write;
					if (bounce == NULL) 
          {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
          }
          memset(bounce, 0, DISK_SECTOR_SIZE);
          
          memcpy(bounce, buffer + bytes_written, write_bytes);
					disk_sector_t new_sector;
					free_map_allocate(1, &new_sector);
					disk_write(filesys_disk, new_sector, bounce);
					int file_length = inode->data.length;
					int logical_file_sector = (file_length+2)/DISK_SECTOR_SIZE;
					if(logical_file_sector <= 12)
					{
							inode->data.file_index[logical_file_sector] = new_sector;
					}

					else if(logical_file_sector>=13 && logical_file_sector<=140)
					{
							if(inode->data.file_index[13] == 0)
							{
									disk_sector_t first_indirection_sector;
									free_map_allocate(1, &first_indirection_sector);
									if(empty_buffer == NULL)
									{
											empty_buffer = malloc(DISK_SECTOR_SIZE);
									}
									memset(empty_buffer, 0, DISK_SECTOR_SIZE);
				
									disk_write(filesys_disk, first_indirection_sector, empty_buffer);
									inode->data.file_index[13] = first_indirection_sector;
							}	
							disk_sector_t one_level_file_index[DISK_SECTOR_SIZE / 4];
							disk_read(filesys_disk, inode->data.file_index[13], one_level_file_index);
							one_level_file_index[logical_file_sector - 13] = new_sector;
				
							disk_write(filesys_disk, inode->data.file_index[13], one_level_file_index);
					}
					else
					{
							if(inode->data.file_index[14] == 0)
							{
									disk_sector_t second_indirection_sector;
									free_map_allocate(1, &second_indirection_sector);
									if(empty_buffer == NULL)
									{
											empty_buffer = malloc(DISK_SECTOR_SIZE);
									}
									memset(empty_buffer, 0, DISK_SECTOR_SIZE);
				
									disk_write(filesys_disk, second_indirection_sector, empty_buffer);
									inode->data.file_index[14] = second_indirection_sector;
							}
							disk_sector_t first_level_file_index[DISK_SECTOR_SIZE / 4];
							disk_read(filesys_disk, inode->data.file_index[14], first_level_file_index);
							int first_level_index = (logical_file_sector - 141) / (DISK_SECTOR_SIZE / 4);
							int second_level_index = (logical_file_sector - 141) % (DISK_SECTOR_SIZE / 4);
							if(first_level_file_index[first_level_index] == 0)
							{
									disk_sector_t first_indirection_sector;
									free_map_allocate(1, &first_indirection_sector);
									if(empty_buffer == NULL)
									{
											empty_buffer = malloc(DISK_SECTOR_SIZE);
									}
									memset(empty_buffer, 0, DISK_SECTOR_SIZE);
				
									disk_write(filesys_disk, first_indirection_sector, empty_buffer);
									first_level_file_index[first_level_index] = first_indirection_sector;
							}
							disk_sector_t second_level_file_index[DISK_SECTOR_SIZE / 4];
							disk_read(filesys_disk, first_level_file_index[first_level_index], second_level_file_index);
							second_level_file_index[second_level_index] = new_sector;
				
							disk_write(filesys_disk, first_level_file_index[first_level_index], second_level_file_index);
				
							disk_write(filesys_disk, inode->data.file_index[14], first_level_file_index);
					}
					/* Advance */

					bytes_to_write -= write_bytes;
					inode->data.length += write_bytes;
					bytes_written += write_bytes;
			}  		
  }  
  free (bounce);
  free (empty_buffer);

  disk_write(filesys_disk, inode->sector, &inode->data);

  return bytes_written;
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
 
//  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
