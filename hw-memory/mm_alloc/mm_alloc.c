/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

//add include
#include <stdbool.h>

// #include "../pintos/src/lib/user/syscall.h"
// #include "../pintos/src/threads/thread.h"
// #include "../pintos/src/lib/kernel/list.h"

struct metadata{
    size_t size;
    bool free;
    struct metadata *prev;
    struct metadata *next;
};
typedef struct metadata Meta;

struct mm_list{
    size_t free_num;
    struct metadata *begin;
    struct metadata *end;
};
typedef struct mm_list List;


void List_init(List* p){
    p->begin = NULL;
    p->end = NULL;
    p->free_num = 0;
}
void List_push(List* p, Meta* node){
    p->end->next = node;
    node->prev = p->end;
    p->end = node;
}
Meta* get_block(List* p,size_t size){
    if(p->free_num != 0){
        Meta* temp = p->begin;
        while(temp != p->end){
            if(temp->size > size){
                return temp;
            }
            temp = temp->next;
        }
    }
}




// struct list* mem_list;

// static bool is_init = false;

// struct Metadata* find_metadata(void* ptr){
//     // return (struct Metadata*)((uint8_t*)ptr - sizeof(struct Metadata));
//     return NULL;
// }

void* mm_malloc(size_t size) {
  //TODO: Implement malloc
  //内存虽然是以页为单位进行申请，但是brk的增加是与increment有关的，
  //只是跨页时要申请新的页，所以brk可以表示当前的占用

    // if(is_init != true){
    //     list_init(mem_list);
    //     is_init = true;
    // }

    if(size ==0)return NULL;
    return NULL;
    

    // for(struct list_elem* temp = list_begin(mem_list);temp != list_end(mem_list); temp = list_next(temp)){
    //     struct Metadata* node = list_entry(temp,struct Metadata, hook);
    //     if((node->free == true) && (size < node->size)){
    //         node->free == false;
    //         void * addr = (uint8_t*)node + sizeof(struct Metadata);
    //         return addr;
    //     }
    // }
    // //如果还能运行到这里，说明找不到node适配，要申请新的node
    // void* res = sbrk(size + sizeof(struct Metadata));
    // if(res == -1)return NULL;//分配失败

    // //初始化
    // struct Metadata* head = (struct Metadata*)res;
    // head->free = false;
    // head->size = size;
    // list_push_back(mem_list,head->hook);

    // void* retaddr = (void*)((uint8_t*)res + sizeof(struct Metadata));

    // return retaddr;
}

void* mm_realloc(void* ptr, size_t size) {
  //TODO: Implement realloc

    if(ptr == NULL){
        return mm_malloc(size);
    }
    return NULL;

    //size < origin
    // struct Metadata* node = find_metadata(ptr);
    // if(size < (node->size - sizeof(struct Metadata))){
    //     //能够重新制作一个metadata header
    //     // node->size = size;

    //     struct Metadata* new = (uint8_t*)ptr + size;
    //     new->size = node->size - size - sizeof(struct Metadata);
    //     new->free = true;
    //     list_push_back(mem_list,new->hook);

    //     node->size = size;
    //     return ptr;//返回原来的ptr就可以了
    // }else{
    //     //重新malloc一块内存返回
    //     void* new = mm_malloc(size);
    //     if(new != NULL){
    //         return new;
    //     }else{
    //         return NULL;
    //     }
    // }
}

void mm_free(void* ptr) {//这个ptr是空间的起始地址，要往下减才能找到metadata
  //TODO: Implement free
    //可以用list entry去找metadata吗？
    // struct Metadata *node = (struct Metadata *)ptr;
    free(ptr);
//     node->free = true;
//     struct list_elem* next_hook = list_next(node->hook);

//     struct Metadata *next = list_entry(next_hook,struct Metadata,hook);
//     if(next->free == true){
//         node->size += next->size;
//         node->hook->next = next->hook->next;
//         memset(next,0,sizeof(struct Metadata));//清空下一块区域的metadata
//     }
//     memset((uint8_t*)node + sizeof(struct Metadata),0,(uint8_t*)node->size -(uint8_t*)node - sizeof(struct Metadata));
}
