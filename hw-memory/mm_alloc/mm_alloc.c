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
    // void *brk;
};
typedef struct mm_list List;

// void *brk;
List mm_list;
bool is_init = false;

void List_init(List* p){
    p->begin = NULL;
    p->end = NULL;
    p->free_num = 0;
}
void List_push(List* p, Meta* node){
    if(p->begin == NULL){
        p->begin = node;
        p->end = node;
    }else{
        p->end->next = node;
        node->prev = p->end;
        p->end = node;
        node->next = NULL;
    }
}


Meta* find_metadata(void* ptr){
    if(ptr == NULL)return NULL;
    return (Meta*)((char*)ptr - sizeof(Meta));
}
void* find_memory(Meta* header){
    if(header == NULL)return NULL;
    return (void*)((char*)header + sizeof(Meta));
}

void set_zero(Meta* header){
    void* start = find_memory(header);
    memset(start,0,header->size);
}
void merge_next(Meta* header){
    header->size = header->size + header->next->size + sizeof(Meta);

    Meta* temp = header->next;
    header->next = temp->next;
    mm_list.free_num--;

    memset(header->next,0,header->next->size);
}
/* 获取block头，alloc只找够大的内存块，不做向后合并的任务 */
Meta* get_block(List* p,size_t size){
    //找空闲块
    if(p->free_num != 0){
        Meta* temp = p->begin;
        while(temp != NULL){
            if((temp->free == true) && temp->size >= size){
                p->free_num--;
                temp->free = false;
                return temp;
            }
            temp = temp->next;
        }        
    }
    //找不到空闲块，申请一块新的
    Meta* new = sbrk(sizeof(Meta) + size);
    printf("new: %p\n",new);
    if(new == (void*)-1)return NULL;

    new->free = false;
    new->size = size;
    set_zero(new);
    List_push(p,new);
    return new;
}

void* mm_malloc(size_t size) {
    if(is_init == false){
        List_init(&mm_list);
        is_init = true;
    }

    if(size == 0)return NULL;

    void* header = get_block(&mm_list,size);
    return find_memory(header);

}
/* 传入空间首地址，重新分配 */
void* mm_realloc(void* ptr, size_t size) {
    if(ptr == NULL){
        return mm_malloc(size);
    }

    // 缩小空间
    Meta* node = find_metadata(ptr);
    if(size <= (node->size - sizeof(Meta))){
        //能够重新制作一个header
        // node->size = size;

        Meta* new = (void*)((char*)ptr + size);
        new->size = node->size - size - sizeof(Meta);
        new->free = true;
        List_push(&mm_list,new);

        node->size = size;
        return ptr;//返回原来的ptr就可以了
    }else{
        //重新malloc一块内存返回
        return mm_malloc(size);
    }
}

/* 传入的是mem的起始地址，要找到header */
void mm_free(void* ptr) {
    if(ptr != NULL){
        Meta* header = find_metadata(ptr);
        if((header->next != NULL) && header->next->free == true){
            merge_next(header);
        }
        header->free = true;
        mm_list.free_num++;
        if((header->prev != NULL) && header->prev->free == true){
            merge_next(header->prev);
        }
    }
}
