#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/thread.h"


/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
  
 
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  disk_sector_t inode_sector = 0;
  /***********Added for project 4**************/
 
  struct dir *dir = get_dir_from_path(name);
  if(dir == NULL)
  	return NULL;
 	
  char *file_name = get_name_from_path(name);
  
  //----------------added code ends-----------------//
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, file_name, inode_sector, false));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  else
  {
  	struct inode *inode = inode_open(inode_sector);
		inode->data.is_dir = 0;
  	inode_close(inode);
  } 
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
	struct inode *inode = NULL;
  /***********Added for project 4**************/
  struct dir *dir = get_dir_from_path(name);
  if(dir == NULL)
  	return NULL;
  
  char *file_name = get_name_from_path(name);
 
  //-----------added code ends--------------------//
  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
	/***********Added for project 4**************/
  struct dir *dir = get_dir_from_path(name);
  if(dir == NULL)
  	return false;
  
  char *file_name = get_name_from_path(name);
  //-----------added code ends--------------------//
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR, 16, "/"))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
/***************Functions added for project 4***********************************/
char *get_highest_dir_from_path(const char *path_)
{
		char *path;
		char *copy_path;
		size_t length = strlen(path_);
		path = (char *)malloc((length+1)*sizeof(char));
		copy_path = (char *)malloc((length+1)*sizeof(char));
		strlcpy(path, path_, length+1);
		strlcpy(copy_path, path_, length+1);
	
		if(strcmp(copy_path, "")==0)
		{
				return NULL;
		}
		char *token, *save_ptr;
		char *ret_val;
		int count = 0;
		
		for (token = strtok_r (copy_path, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
    {count++;}
    
    if(count == 1 || count == 0)
    		return NULL;
    
    token = strtok_r (path, "/", &save_ptr);
    ret_val = (char *)malloc((strlen(token)+1)*sizeof(char));
    strlcpy(ret_val, token, (strlen(token)+1));
    
    free(path);
    free(copy_path);

    return ret_val;		
   	
}
char *get_name_from_path(const char *path_)
{
		
		char *path;
		char *copy_path;
		size_t length = strlen(path_);
		path = (char *)malloc((length+1)*sizeof(char));
		copy_path = (char *)malloc((length+1)*sizeof(char));
		strlcpy(path, path_, length+1);
		strlcpy(copy_path, path_, length+1);
		
		if(strcmp(path, "")==0)
		{
				return "";
		}
		char *token, *save_ptr;
		char *ret_val;
		int count = 0;
	
   	for (token = strtok_r (copy_path, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
    {
    		count++;
 
    }
    int i = 0;
    
    for (token = strtok_r (path, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
    {
    		count--;
 
    		if(count==0)
    		  break;
    }
    
    ret_val = (char *)malloc((strlen(token)+1)*sizeof(char));
    strlcpy(ret_val, token, (strlen(token)+1));
    free(path);
    free(copy_path);
    
    return ret_val;
}
struct dir *get_dir_from_path(const char *name_)
{
	struct dir *dir;
	char *name = name_;
	char *copy_name = name;

	if(name[0] == '/')
		dir = dir_open_root ();
  else
  	dir = dir_open(inode_open(thread_current()->cur_dir));		

  
	char *dir_name;
	struct inode *inode;
	
	while( ( dir_name = get_highest_dir_from_path(name) ) != NULL)
 	{
 		//printf("/%s", dir_name);
  	if(dir_lookup(dir, dir_name, &inode))
  	{
  		dir_close(dir);
  		dir = dir_open(inode);
  	}
  	else
  	{
  		return NULL;
  	}	
  	int i;
  	int set_count = 0, slash_count = 0;
  	if(name[0] == '/')
  		set_count = 2;
  	else
  		set_count = 1;
  	
  	for(i=0;;i++)
  	{
  		if(name[i] == '/')
  			slash_count ++;
  		if(slash_count == set_count)	
  			break;
  	}
 		
  	name = name + i;	
 
  }
 //	printf("\n");

  return dir;
}

