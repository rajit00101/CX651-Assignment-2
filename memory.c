#include "memory.h"
#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>

typedef struct header {
    size_t size;
    struct header * prev;
    struct header * next;
    int in_use;
} m_header;

m_header* freelist = NULL;

void print_freelist() {
    m_header* curr = freelist;
    while(curr != NULL) {
        printf("[%p: size:%lu prev:%p next:%p use:%d]\n", curr, curr->size, curr->prev, curr->next, curr->in_use);
        curr = curr->next;
    }
    printf("\n --- \n");
}

void * new_malloc(size_t size) {
    if(freelist == NULL) {
        printf("MMAP\n");
         // Use mmap to get anonymous, private memory
        freelist = mmap(NULL,                    // Desired start address (NULL lets OS choose)
                      2048,                  // Length of the mapping (rounded up to page size)
                      PROT_READ | PROT_WRITE,  // Memory protection: readable and writable
                      MAP_PRIVATE | MAP_ANONYMOUS, // Visibility: private to the process, not file-backed
                      -1,                      // File descriptor: -1 for anonymous mapping
                      0);                      // Offset: 0 for anonymous mapping

        if (freelist == MAP_FAILED) {
            printf("map failed\n");
            return NULL;
        }

        freelist->size = 2048 - sizeof(m_header);
        freelist->prev = NULL;
        freelist->next = NULL;
        freelist->in_use = 0;


    }

    m_header* curr = freelist;
    while(curr != NULL) {
        if(curr->in_use == 0 && curr->size >= size) {
            if(curr->size >= size + sizeof(m_header) + 1) {
                m_header* new_block = (m_header*)((char*)(curr + 1) + size);
                new_block->size = curr->size - size - sizeof(m_header);
                new_block->prev = curr;
                new_block->next = curr->next;
                new_block->in_use = 0;

                if(curr->next != NULL) {
                    curr->next->prev = new_block;
                }

                curr->next = new_block;
                curr->size = size;
            }

            curr->in_use = 1;
            return (void*)(curr + 1);
        }

        curr = curr->next;
    }

    return NULL;
}

void new_free(void * ptr) {

    if(ptr == NULL) {
        return;
    }
    m_header* block = ((m_header*)ptr) - 1;
    block -> in_use = 0;

}