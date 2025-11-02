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
void print_block_list(List* p)
{
    if(p == NULL)return;

    Meta* temp = p->begin;
    for(int i = 0; temp != NULL; i++){
        printf("i = %d, size = %ld, free = %d\n",i,temp->size,temp->free);
        temp = temp->next;
    }
    printf("\n");
    
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

    Meta* next_blk = header->next;
    if(next_blk->next != NULL){
        header->next = next_blk->next;
        next_blk->next->prev = header;
    }else{
        mm_list.end = header;
        header->next = NULL;
    }
    mm_list.free_num--;

    // memset(header->next,0,header->next->size);
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

    print_block_list(&mm_list);
    return find_memory(header);

}
/*  传入空间首地址，重新分配
    建议的实现方式是先 mm_malloc 一个指定大小的块，
    将旧数据 memcopy 到新块，然后在最后调用 mm_free(ptr)。*/
void* mm_realloc(void* ptr, size_t size) {
    if(ptr == NULL){
        return mm_malloc(size);
    }
    if(size == 0){
        mm_free(ptr);
        return NULL;
    }

    Meta* header = find_metadata(ptr);
    if(size == header->size){
        return ptr;
    }

    //搬运到新的block
    void* new = mm_malloc(size);
    if(new == NULL){
        return NULL;
    }
    // void* mm_new = find_memory(new);
    memcpy(new,ptr,header->size);

    mm_free(ptr);
    // header->free = true;//释放旧的块
    print_block_list(&mm_list);
    return new;
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
    print_block_list(&mm_list);
}

