/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "../pintos/src/lib/user/syscall.h"
// #include "../pintos/src/threads/thread.h"
#include "../pintos/src/lib/kernel/list.h"

struct mem_list{
    struct list_elem node;
    void* addr;//空间的起始位置
    uint16_t size;
};

struct list* list;

static bool is_init = false;

void* init_mem_block(void* start){
    struct mem_list t;

    // t->addr = (void*)((uint8_t)start + size)
    

}

void* mm_malloc(size_t size) {
  //TODO: Implement malloc
  //内存虽然是以页为单位进行申请，但是brk的增加是与increment有关的，
  //只是跨页时要申请新的页，所以brk可以表示当前的占用

    if(is_init != true){
        list_init(list);
        is_init = true;
    }


  



  

  return NULL;
}

void* mm_realloc(void* ptr, size_t size) {
  //TODO: Implement realloc
    if(is_init != true){
        list_init(list);
        is_init = true;
    }

  return NULL;
}

void mm_free(void* ptr) {
  //TODO: Implement free
    if(is_init != true){
        list_init(list);
        is_init = true;
    }
}
