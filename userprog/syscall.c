#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "process.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"



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
  //printf("system create!,file =  %s && size = %d\n",file,initial_size);
  struct thread *t = thread_current();
  if (file != NULL) 
    return filesys_create(file,initial_size);
  else
    thread_exit();
}

bool sys_remove (const char *file){
  //printf("system remove!\n");
  return filesys_remove(file);
}


int sys_open (const char *file){
  //printf("system open!, file = '%s' \n", file);
  int i = 2 ; //reserve 0 & 1 for STDIN & STDOUT
  struct thread *t = thread_current();
  if (t->fd == NULL) {
    t->fd = malloc(128*sizeof(struct file *)); 
    for(i = 2; i < 128; i++)
      t->fd[i] = NULL;
  }
  if (file != NULL) {
    if(file[0] == NULL)
      return -1;
    struct file *f = filesys_open(file);
    if(f == NULL)
      return -1 ;
    i = 2;
    while(t->fd[i] != NULL) {
      i++;
    }  
    t->fd[i] = f;
    return i ;
  } else {
    thread_exit();
  }
}

int sys_filesize (int fdes){
  struct thread *t = thread_current();
  //printf("system filesize!\n");
  if (fdes < 128 && fdes > 1 && t->fd != NULL && t->fd[fdes] != NULL)
    return file_length(t->fd[fdes]);
  else
    return -1;
}

int sys_read (int fdes, void *buffer, unsigned size){
  //printf("system read!\n fd = %d && size = %d\n",fdes,size);
  struct thread *t = thread_current();
  if (fdes == 1) 
    thread_exit();
  if (fdes == 0) {
    return size ;
  } else {
      if (fdes < 128 && fdes > 1 && t->fd != NULL && t->fd[fdes] != NULL) 
	return file_read(t->fd[fdes],buffer,size); 
      else
	thread_exit();
  }
}

int sys_write (int fdes, const void *buffer, unsigned size){
  struct thread *t = thread_current();
  //printf("system write! %d \n", fdes);
  if(fdes == 1){
    putbuf((char*)(buffer), size);  
    return size;
  } else {
    if (fdes < 128 && fdes > 1 && t->fd != NULL && t->fd[fdes] != NULL)
      return file_write(t->fd[fdes],buffer,size);
    else
      thread_exit();
  }
  return -1;
}

void sys_seek (int fdes, unsigned position){
  //printf("system seek!\n");
  struct thread *t = thread_current();
  if (t->fd != NULL && fdes < 128 && fdes > 1 && t->fd[fdes] != NULL)
    file_seek(t->fd[fdes],position);
}

unsigned sys_tell (int fdes){
  //printf("system tell!\n");
  struct thread *t = thread_current();
  if (t->fd != NULL && fdes < 128 && fdes > 1 && t->fd[fdes] != NULL)
    return file_tell(t->fd[fdes]);
  return -1 ;
}

void sys_close (int fdes){
  //printf("system close!\n  fdes = %d \n",fdes);
  if(fdes == 0 || fdes == 1) {
    return ;    
  }
  struct thread *t = thread_current();
  if (fdes < 128 && fdes > 1 && t->fd != NULL && t->fd[fdes] != NULL) {
    file_close(t->fd[fdes]);
    t->fd[fdes] = NULL ;
  } else {
    thread_exit();
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf("esp = %x \n", f->esp); 
  //hex_dump(f->esp, f->esp, 20, true);
  struct thread *t = thread_current();
  if(is_user_vaddr(f->esp) && pagedir_get_page(t->pagedir,f->esp) == NULL)
    thread_exit();  
  int sys_num = *((int*)(f->esp));
  int arg0 = *((int*)(f->esp)+1); 
  int arg1 = *((int*)(f->esp)+2);
  int arg2 = *((int*)(f->esp)+3);


  switch(sys_num){
  case SYS_HALT:
    sys_halt(); break;
  case SYS_EXIT:
    if(!is_user_vaddr((int*)(f->esp)+1) || pagedir_get_page(t->pagedir,(int*)(f->esp)+1) == NULL)
      thread_exit(); 
    sys_exit(arg0); break;
  case SYS_EXEC:
    if(!is_user_vaddr(arg0) || pagedir_get_page(t->pagedir,arg0) == NULL)
      thread_exit(); 
    f->eax = sys_exec((char*)arg0); break;
  case SYS_WAIT:
    f->eax = sys_wait(arg0); break;
  case SYS_CREATE:
    if(!is_user_vaddr(arg0) || pagedir_get_page(t->pagedir,arg0) == NULL)
      thread_exit(); 
    f->eax = sys_create((char*)arg0,(unsigned)arg1); break;
  case SYS_REMOVE:
    if(!is_user_vaddr(arg0) || pagedir_get_page(t->pagedir,arg0) == NULL)
      thread_exit(); 
    f->eax = sys_remove((char*)arg0); break;
  case SYS_OPEN:
    if(!is_user_vaddr(arg0) || pagedir_get_page(t->pagedir,arg0) == NULL)
      thread_exit(); 
    f->eax = sys_open((char*)arg0); break;
  case SYS_FILESIZE:
    f->eax = sys_filesize(arg0); break;
  case SYS_READ:
    if(!is_user_vaddr(arg1))
      thread_exit();
    if((unsigned)arg1 >= (unsigned)(f->esp)-32){
      int numPages = 0;
      //printf("growing stack in syscall %x, %x, %d\n", arg1, f->esp, arg2);
      while(numPages*PGSIZE <= pg_round_down(arg1+arg2)-pg_round_down(arg1)){
	if(pagedir_get_page(t->pagedir,arg1+numPages*PGSIZE) == NULL){
	  uint8_t *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	  install_page(pg_round_down((void*)(arg1+numPages*PGSIZE)), kpage, true);
	  struct thread *t = thread_current();
	  invalidate_pagedir(t->pagedir);
	}
	numPages += 1;
      }
      f->eax = sys_read(arg0,(void*)arg1,(unsigned)arg2); break;
    }
    
    int i;
    struct segment seg;
    for(i=0; i<3; i++){
      seg = t->segment_table[i];
      if(seg.upage <= (void*)(arg1) && (void*)(arg1) <= seg.upage + seg.read_bytes + seg.zero_bytes){
	int numPages = 0;
	while(numPages*PGSIZE <= (unsigned)arg2 + PGSIZE){
	  if(pagedir_get_page(t->pagedir,arg1+numPages*PGSIZE) == NULL){
	    uint8_t *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	    
	    if(pg_round_down((void*)(arg1+numPages*PGSIZE)) < seg.upage){
	      file_read_at(seg.file, kpage,(uint8_t*)pg_round_up(seg.upage)-seg.upage, seg.ofs);
	    } else if(pg_round_up((void*)(arg1+numPages*PGSIZE)) > seg.upage + seg.read_bytes){
	      file_read_at(seg.file, kpage, seg.upage+seg.read_bytes-(uint8_t*)pg_round_down((void*)(arg1+numPages*PGSIZE)), seg.ofs+(uint8_t*)pg_round_down((void*)(arg1+numPages*PGSIZE))-seg.upage);
	    } else{
	      file_read_at(seg.file, kpage, PGSIZE, seg.ofs+(uint8_t*)pg_round_down((void*)(arg1+numPages*PGSIZE))-seg.upage);
	    }
	    
	    //printf("set page in syscall at %x, %x, %x, %d \n", pg_round_down((void*)(arg1+numPages*PGSIZE)),(void*)(arg1+numPages*PGSIZE),kpage,seg.writable);
	    pagedir_set_page(t->pagedir, pg_round_down((void*)(arg1+numPages*PGSIZE)), kpage, seg.writable);
	  }
	  numPages += 1;
	}
	invalidate_pagedir(t->pagedir);
	
	f->eax = sys_read(arg0,(void*)arg1,(unsigned)arg2); return;
      } 
    }
      
    thread_exit();
  case SYS_WRITE:
    if(!is_user_vaddr(arg1) || pagedir_get_page(t->pagedir,arg1) == NULL)
      thread_exit(); 
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
