#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "process.h"
#include "filesys/file.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void sys_halt(){
  //printf("system halt!\n");
  shutdown_power_off();
}

void sys_exit(int status){
  struct thread *t = thread_current();
  t->metastatus = status;
  //  printf("system exit!, %d \n", t->metastatus);
  thread_exit();
}
  
int sys_exec(const char *file){ // make return pid_t
  //printf("system exec!, %s \n",file);
  return process_execute(file);
  
}

int sys_wait (int pid){ // make argument pid_t
  //printf("system wait!, %d \n", pid);
  return process_wait(pid);
}

bool sys_create (const char *file, unsigned initial_size){
  //printf("system create!\n");
  return filesys_create(file,initial_size);
}

bool sys_remove (const char *file){
  //printf("system remove!\n");
  return filesys_remove(file);
}


int sys_open (const char *file){
  //printf("system open!\n");
  int i = 2 ; //reserve 0 & 1 for STDIN & STDOUT
  struct thread *t = thread_current();
  if (t->fd == NULL) {
    t->fd = malloc(128*sizeof(struct file *));  
  }
  struct file *f = filesys_open(file);
  while(t->fd[i] != NULL) {
    i++;
  }  
  t->fd[i] = f;
  return i ;
}

int sys_filesize (int fdes){
  struct thread *t = thread_current();
  //printf("system filesize!\n");
  if (t->fd[fdes] == NULL || fdes > 128 || fdes < 2)
	  return -1 ;
  else
    return file_length(t->fd[fdes]);
}

int sys_read (int fdes, void *buffer, unsigned size){
  //printf("system read!\n");
  struct thread *t = thread_current();
  if (fdes == 0) {
    //input_getc(fdes);
    return size ;
  } else {
      if (t->fd[fdes] != NULL && fdes < 128 && fdes > 1) 
	return file_read(t->fd[fdes],buffer,size);      
  }
  return -1 ;
}

int sys_write (int fdes, const void *buffer, unsigned size){
  struct thread *t = thread_current();
  //printf("system write! %d \n", fdes);
  if(fdes == 1){
    putbuf((char*)(buffer), size);  
    return size;
  } else {
    if (t->fd[fdes] != NULL && fdes < 128 && fdes > 1)
      return file_write(t->fd[fdes],buffer,size);
  }
  return -1;
}

void sys_seek (int fdes, unsigned position){
  //printf("system seek!\n");
  struct thread *t = thread_current();
  if (t->fd[fdes] != NULL && fdes < 128 && fdes > 1)
    file_seek(t->fd[fdes],position);
}

unsigned sys_tell (int fdes){
  //printf("system tell!\n");
  struct thread *t = thread_current();
  if (t->fd[fdes] != NULL && fdes < 128 && fdes > 1)
    return file_tell(t->fd[fdes]);
  return -1 ;
}

void sys_close (int fdes){
  // printf("system close!\n");
  struct thread *t = thread_current();
  if (t->fd[fdes] != NULL && fdes < 128 && fdes > 1) 
    t->fd[fdes] = NULL ;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // hex_dump(f->esp, f->esp, 20, true);
  int sys_num = *((int*)(f->esp));
  int arg0 = *((int*)(f->esp)+1);
  int arg1 = *((int*)(f->esp)+2);
  int arg2 = *((int*)(f->esp)+3);

  switch(sys_num){
  case SYS_HALT:
    sys_halt(); break;
  case SYS_EXIT:
    sys_exit(arg0); break;
  case SYS_EXEC:
    f->eax = sys_exec((char*)arg0); break;
  case SYS_WAIT:
    f->eax = sys_wait(arg0); break;
  case SYS_CREATE:
    sys_create((char*)arg0,(unsigned)arg1); break;
  case SYS_REMOVE:
    sys_remove((char*)arg0); break;
  case SYS_OPEN:
    f->eax = sys_open((char*)arg0); break;
  case SYS_FILESIZE:
    f->eax = sys_filesize(arg0); break;
  case SYS_READ:
    f->eax = sys_read(arg0,(void*)arg1,(unsigned)arg2); break;
  case SYS_WRITE:
    f->eax = sys_write(arg0,(void*)arg1,(unsigned)arg2); break;
  case SYS_SEEK:
    sys_seek(arg0,(unsigned)arg1); break;
  case SYS_TELL:
    f->eax = sys_tell(arg0); break;
  case SYS_CLOSE:
    sys_close(arg0); break;
  default:  
    printf ("system call!\n");
    thread_exit ();
    break;
  }

}
