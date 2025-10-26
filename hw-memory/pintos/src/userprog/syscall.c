#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

void syscall_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

/*
 * This does not check that the buffer consists of only mapped pages; it merely
 * checks the buffer exists entirely below PHYS_BASE.
 */
static void validate_buffer_in_user_region(const void* buffer, size_t length) {
  uintptr_t delta = PHYS_BASE - buffer;
  if (!is_user_vaddr(buffer) || length > delta)
    syscall_exit(-1);
}

/*
 * This does not check that the string consists of only mapped pages; it merely
 * checks the string exists entirely below PHYS_BASE.
 */
static void validate_string_in_user_region(const char* string) {
  uintptr_t delta = PHYS_BASE - (const void*)string;
  if (!is_user_vaddr(string) || strnlen(string, delta) == delta)
    syscall_exit(-1);
}

static int syscall_open(const char* filename) {
  struct thread* t = thread_current();
  if (t->open_file != NULL)
    return -1;

  t->open_file = filesys_open(filename);
  if (t->open_file == NULL)
    return -1;

  return 2;
}

static int syscall_write(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  } else if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_write(t->open_file, buffer, size);
}

static int syscall_read(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_read(t->open_file, buffer, size);
}

static void syscall_close(int fd) {
  struct thread* t = thread_current();
  if (fd == 2 && t->open_file != NULL) {
    file_close(t->open_file);
    t->open_file = NULL;
  }
}

/*  您应该确保进程的堆位于进程代码和其他从可执行文件加载的数据之上
   （即虚拟地址高于该地址）。您应该确定程序加载时堆的起始地址，
    并在加载后在整个进程运行过程中保持固定。

    以下是如何将新页面映射到进程的虚拟地址空间的方法。
    1. 使用 palloc_get_page 并传递 PAL_USER 标志从用户池中分配页面。
    2. 将页面内容清零，方法是使用 memset 或在分配页面时传递 PAL_ZERO 标志
      （例如 palloc_get_page(PAL_ZERO | PAL_USER)）。
    3. 使用 pagedir_set_page 将页面映射到进程的虚拟地址空间。
    4. 当进程退出并调用 pagedir_destroy 时，页面将被释放。或者，
      如果您希望在其他时间释放页面，可以使用 pagedir_clear_page 将其从页表中删除，
      然后稍后使用 palloc_free_page 释放它。

      函数会将中断点的位置递增一个字节，并返回上一个中断点的地址（即，如果增量为正数，则返回新映射内存的起始地址）。
      要获取中断点的当前位置，请传入增量 0。
*/
static void* syscall_sbrk(intptr_t increment){
    struct thread *current = thread_current();
    void *heap_start = current->heap_start;
    void *old_brk = current->heap_brk;

    /* 如果用户程序移动段分隔符是为了增加堆的大小，
    您应该根据需要分配页面并将其映射到用户的虚拟地址空间。
    如果用户程序移动段分隔符是为了减少堆的大小，
    您应该根据需要释放不再包含堆部分的页面。
    只有当段分隔符跨越页面边界时，才需要分配或释放页面。*/
    if(increment == 0){
        return current->heap_brk;
    }

    void *new_brk = (void*)((uint8_t*)old_brk + increment);
    if(new_brk < heap_start){
        return (void*)-1;
    }
    if(new_brk >= (void*)PHYS_BASE){
        return (void*)-1;
    }

    void* old_page = pg_round_up(old_brk);
    void* new_page = pg_round_up(new_brk);

    if(old_page != new_page){
        if(increment > 0){//向上增长
                for(void* current_page = old_page;current_page < new_page; 
                    current_page = (void*)((uint8_t*)current_page + PGSIZE)){
                        void* npage = palloc_get_page(PAL_ZERO | PAL_USER);
                        if(npage == NULL){/*TODO: 分配失败时把之前的page全都释放*/
                            printf("npage alloc failed\n");
                            return (void*)-1;
                        }
                        if(!pagedir_set_page(current->pagedir,current_page,npage,true)){
                            palloc_free_page(npage);
                            return (void*)-1;
                        }
                }

        }else{//不在同一页，要进行删除操作
                for(void* current_page = old_page; current_page > new_page; 
                    current_page = (void*)((uint8_t*)current_page - PGSIZE)){
                    //找到当前page在kernel 虚拟地址的映射，清除标志位然后free_page
                    void* kpage = pagedir_get_page(current->pagedir,current_page);
                    if(kpage != NULL){
                        pagedir_clear_page(current->pagedir,current_page);
                        palloc_free_page(kpage);
                    }
                }
        }
    }
    //一致更新brk并返回旧brk
    current->heap_brk = new_brk;
    return old_brk;
}

static void syscall_handler(struct intr_frame* f) {
  uint32_t* args = (uint32_t*)f->esp;
  struct thread* t = thread_current();
  t->in_syscall = true;

  validate_buffer_in_user_region(args, sizeof(uint32_t));
  switch (args[0]) {
    case SYS_EXIT:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_exit((int)args[1]);
      break;

    case SYS_OPEN:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      validate_string_in_user_region((char*)args[1]);
      f->eax = (uint32_t)syscall_open((char*)args[1]);
      break;

    case SYS_WRITE:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_write((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_READ:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_read((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_CLOSE:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_close((int)args[1]);
      break;

    case SYS_SBRK:
      validate_buffer_in_user_region(&args[1],sizeof(intptr_t));
      f->eax = (uint32_t)syscall_sbrk((intptr_t)args[1]);
      break;
      
      

    default:
      printf("Unimplemented system call: %d\n", (int)args[0]);
      break;
  }

  t->in_syscall = false;
}
