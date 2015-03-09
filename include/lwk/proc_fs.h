/** \file
 * Kernel virtual proc filesystem
 */
#ifndef _lwk_proc_fs_h_
#define _lwk_proc_fs_h_

int create_proc_file(char * path, 
		     int (*get_proc_data)(struct file * file, void * priv_data),
		     void * priv_data);

int remove_proc_file(char * path);

int proc_mkdir(char * dir_name);
int proc_rmdir(char * dir_name);

int proc_sprintf(struct file * file, const char * fmt, ...);


#endif
