#include "memory.h"
#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#define MEMORY_SIZE 2048
#define ALIGNMENT 16

typedef struct header {
    size_t size;
    struct header *prev;
    struct header *next;
    int in_use;
} m_header;

m_header *freelist = NULL;

void print_freelist(void) {
    m_header *curr = freelist;
    while (curr != NULL) {
        printf("[%p: size:%lu prev:%p next:%p use:%d]\n",
               (void *)curr, (unsigned long)curr->size,
               (void *)curr->prev, (void *)curr->next, curr->in_use);
        curr = curr->next;
    }
    printf("\n --- \n");
}

static size_t align16(size_t size) {
    if (size > SIZE_MAX - (ALIGNMENT - 1)) {
        return 0;
    }
    return (size + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1);
}

static int initialize_memory(void) {
    freelist = mmap(NULL,
                    MEMORY_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1,
                    0);

    if (freelist == MAP_FAILED) {
        freelist = NULL;
        return 0;
    }

    freelist->size = MEMORY_SIZE - sizeof(m_header);
    freelist->prev = NULL;
    freelist->next = NULL;
    freelist->in_use = 0;
    return 1;
}

void *new_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size = align16(size);
    if (size == 0) {
        return NULL;
    }

    if (freelist == NULL && !initialize_memory()) {
        return NULL;
    }

    m_header *curr = freelist;

    while (curr != NULL) {
        if (!curr->in_use && curr->size >= size) {
            size_t remaining = curr->size - size;

            /* Keep all payloads 16-byte aligned after a split. */
            if (remaining >= sizeof(m_header) + ALIGNMENT) {
                m_header *new_block =
                    (m_header *)((char *)(curr + 1) + size);

                new_block->size = remaining - sizeof(m_header);
                new_block->prev = curr;
                new_block->next = curr->next;
                new_block->in_use = 0;

                if (curr->next != NULL) {
                    curr->next->prev = new_block;
                }

                curr->next = new_block;
                curr->size = size;
            }

            curr->in_use = 1;
            return (void *)(curr + 1);
        }

        curr = curr->next;
    }

    return NULL;
}

void new_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    m_header *curr = ((m_header *)ptr) - 1;
    curr->in_use = 0;

    /* Merge with the next block first. */
    if (curr->next != NULL && !curr->next->in_use) {
        m_header *next = curr->next;
        curr->size += sizeof(m_header) + next->size;
        curr->next = next->next;

        if (curr->next != NULL) {
            curr->next->prev = curr;
        }
    }

    /* Then merge with the previous block if it is free. */
    if (curr->prev != NULL && !curr->prev->in_use) {
        m_header *prev = curr->prev;
        prev->size += sizeof(m_header) + curr->size;
        prev->next = curr->next;

        if (prev->next != NULL) {
            prev->next->prev = prev;
        }
    }
}
