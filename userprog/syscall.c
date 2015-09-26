#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void sys_halt(){
  printf("system halt!\n");
  thread_exit();
}

void sys_exit(int status){
  printf("system exit!\n");
  struct thread *t = thread_current();
  t->status = status;
  thread_exit();
}
  
int sys_exec(const char *file){ // make return pid_t
  printf("system exec!, %s \n",file);
  /* tid_t tid = process_execute(file);
  enum intr_level level = intr_disable();
  struct thread *t = thread_by_tid(tid);
  intr_set_level(level);
  if(tid != -1){
    sema_down(&t->parent);
    if(t->start)
      return tid;
      }*/
  while(1){};
  return -1;
}

int sys_wait (int pid){ // make argument pid_t
  printf("system wait!\n");
  return process_wait(pid);
}

bool sys_create (const char *file, unsigned initial_size){
  printf("system create!\n");
  thread_exit();
  return 0;
}

bool sys_remove (const char *file){
  printf("system remove!\n");
  thread_exit();
  return 0;
}


int sys_open (const char *file){
  printf("system open!\n");
  thread_exit();
  return 0;
}

int sys_filesize (int fd){
  printf("system filesize!\n");
  thread_exit();
  return 0;
}

int sys_read (int fd, void *buffer, unsigned size){
  printf("system read!\n");
  thread_exit();
  return 0;
}

int sys_write (int fd, const void *buffer, unsigned size){
  if(fd == 1){
    putbuf((char*)(buffer), size);  
    return size;
  } 

  printf("system write!\n");
  while(1){};
  thread_exit();
  return 0;
}

void sys_seek (int fd, unsigned position){
  printf("system seek!\n");
  thread_exit();
}

unsigned sys_tell (int fd){
  printf("system tell!\n");
  thread_exit();
  return 0;
}

void sys_close (int fd){
  printf("system close!\n");
  thread_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int sys_num = *((int*)(f->esp));
  int arg0 = ((int*)(f->esp)+1);
  int arg1 = ((int*)(f->esp)+2);
  int arg2 = ((int*)(f->esp)+3);

  switch(sys_num){
  case SYS_HALT:
    sys_halt(); break;
  case SYS_EXIT:
    sys_exit(arg0); break;
  case SYS_EXEC:
    sys_exec((char*)arg0); break;
  case SYS_WAIT:
    sys_wait(arg0); break;
  case SYS_CREATE:
    sys_create((char*)arg0,(unsigned)arg1); break;
  case SYS_REMOVE:
    sys_remove((char*)arg0); break;
  case SYS_OPEN:
    sys_open((char*)arg0); break;
  case SYS_FILESIZE:
    sys_filesize(arg0); break;
  case SYS_READ:
    sys_read(arg0,(void*)arg1,(unsigned)arg2); break;
  case SYS_WRITE:
    sys_write(arg0,(void*)arg1,(unsigned)arg2); break;
  case SYS_SEEK:
    sys_seek(arg0,(unsigned)arg1); break;
  case SYS_TELL:
    sys_tell(arg0); break;
  case SYS_CLOSE:
    sys_close(arg0); break;
  default:  
    printf ("system call!\n");
    thread_exit ();
    break;
  }

}
