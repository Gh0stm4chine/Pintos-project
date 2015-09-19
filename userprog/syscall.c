#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  hex_dump(f->esp,f->esp,20,true);
  int *arg0; int *arg1; int *arg2; 
  int sys_num = *((int*)(f->esp));
  printf("%d \n",sys_num);
  switch(sys_num){
  case SYS_HALT:
    break;
  case SYS_CREATE:
  case SYS_SEEK:
    *arg0 = *(((int*)(f->esp))+1);
    *arg1 = *(((int*)(f->esp))+2);
    break;
  case SYS_READ:
  case SYS_WRITE:
    *arg0 = *(((int*)(f->esp))+1);
    *arg1 = *(((int*)(f->esp))+2);
    *arg2 = *(((int*)(f->esp))+3);
    break;
  default:
    *arg0 = *(((int*)(f->esp))+1);
    break;
  }
 
  switch(sys_num){
  case SYS_HALT:
    sys_halt(); break;
  case SYS_EXIT:
    sys_exit(*arg0); break;
  case SYS_EXEC:
    sys_exec((char*)arg0); break;
  case SYS_WRITE:
    //sys_write(*arg0, (void*)arg1, (unsigned)(*arg2)); break;
  default:
    break;
  }
  printf ("system call!\n");
  thread_exit ();
}

void sys_halt(){
  printf("system halt!\n");
  thread_exit();
}

void sys_exit(int status){
  printf("system exit!\n");
  thread_exit();
}
  
int sys_exec(const char *file){ // make return pid_t
  printf("system exec!\n");
  thread_exit();
  return 0;
}

int sys_wait (int pid){ // make argument pid_t
  printf("system wait!\n");
  thread_exit();
  return 0;
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
  printf("system write!\n");
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
