/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MEM_H_INCLUDED
#define ABTI_MEM_H_INCLUDED

// FULT
#define ABT_CONFIG_USE_MEM_POOL
typedef struct fthread ABTI_thread;
typedef int ABT_bool;
typedef struct ABTI_stack_header    ABTI_stack_header;
typedef struct ABTI_page_header     ABTI_page_header;
typedef struct ABTI_sp_header       ABTI_sp_header;

struct ABTI_local {
    uint32_t num_stacks;                /* Current # of stacks */
    ABTI_stack_header *p_mem_stack;     /* Free stack list */
    ABTI_page_header *p_mem_task_head;  /* Head of page list */
    ABTI_page_header *p_mem_task_tail;  /* Tail of page list */
};

struct ABTI_global {
  uint32_t cache_line_size;          /* Cache line size */
  // uint32_t os_page_size;             /* OS page size */
  uint32_t huge_page_size;           /* Huge page size */
  uint32_t mem_page_size;            /* Page size for memory allocation */
  uint32_t mem_sp_size;              /* Stack page size */
  uint32_t mem_max_stacks;           /* Max. # of stacks kept in each ES */
  int mem_lp_alloc;                  /* How to allocate large pages */
  uint32_t mem_sh_size;              /* Stack header (including ABTI_thread
                                        ABTI_stack_header) size */
  ABTI_stack_header *p_mem_stack;    /* List of ULT stack */
  ABTI_page_header *p_mem_task;      /* List of task block pages */
  ABTI_sp_header *p_mem_sph;         /* List of stack pages */
  volatile int lock;
};

typedef struct ABTI_global ABTI_global;
typedef struct ABTI_local ABTI_local;

extern __thread ABTI_local *lp_ABTI_local;
extern ABTI_global *gp_ABTI_global;

#define ABT_FALSE 0
#define ABT_TRUE 1

/* Memory allocation */

#define ABTU_CA_MALLOC(s)   malloc(s)

#ifdef ABT_CONFIG_USE_MEM_POOL
typedef struct ABTI_blk_header  ABTI_blk_header;

enum {
    ABTI_MEM_LP_MALLOC = 0,
    ABTI_MEM_LP_MMAP_RP,
    ABTI_MEM_LP_MMAP_HP_RP,
    ABTI_MEM_LP_MMAP_HP_THP,
    ABTI_MEM_LP_THP
};

struct ABTI_sp_header {
    uint32_t num_total_stacks;  /* Number of total stacks */
    uint32_t num_empty_stacks;  /* Number of empty stacks */
    size_t stacksize;           /* Stack size */
    uint64_t id;                /* ID */
    ABT_bool is_mmapped;        /* ABT_TRUE if it is mmapped */
    void *p_sp;                 /* Pointer to the allocated stack page */
    ABTI_sp_header *p_next;     /* Next stack page header */
};

struct ABTI_stack_header {
    ABTI_stack_header *p_next;
    ABTI_sp_header *p_sph;
    void *p_stack;
};

struct ABTI_page_header {
    uint32_t blk_size;          /* Block size in bytes */
    uint32_t num_total_blks;    /* Number of total blocks */
    uint32_t num_empty_blks;    /* Number of empty blocks */
    uint32_t num_remote_free;   /* Number of remote free blocks */
    ABTI_blk_header *p_head;    /* First empty block */
    ABTI_blk_header *p_free;    /* For remote free */
    // ABTI_xstream *p_owner;      /* Owner ES */
    ABTI_page_header *p_prev;   /* Prev page header */
    ABTI_page_header *p_next;   /* Next page header */
    ABT_bool is_mmapped;        /* ABT_TRUE if it is mmapped */
};

struct ABTI_blk_header {
    ABTI_page_header *p_ph;     /* Page header */
    ABTI_blk_header *p_next;    /* Next block header */
};

void ABTI_mem_init(ABTI_global *p_global);
void ABTI_mem_init_local(ABTI_local *p_local);
void ABTI_mem_finalize(ABTI_global *p_global);
void ABTI_mem_finalize_local(ABTI_local *p_local);
int ABTI_mem_check_lp_alloc(int lp_alloc);

char *ABTI_mem_take_global_stack(ABTI_local *p_local);
void ABTI_mem_add_stack_to_global(ABTI_stack_header *p_sh);
ABTI_page_header *ABTI_mem_alloc_page(ABTI_local *p_local, size_t blk_size);
void ABTI_mem_free_page(ABTI_local *p_local, ABTI_page_header *p_ph);
void ABTI_mem_take_free(ABTI_page_header *p_ph);
void ABTI_mem_free_remote(ABTI_page_header *p_ph, ABTI_blk_header *p_bh);
ABTI_page_header *ABTI_mem_take_global_page(ABTI_local *p_local);

char *ABTI_mem_alloc_sp(ABTI_local *p_local, size_t stacksize);

/* ABTI_EXT_STACK means that the block will not be managed by ES's stack pool
 * and it needs to be freed with ABTU_free. */
#define ABTI_EXT_STACK      (ABTI_stack_header *)(UINTPTR_MAX)

/******************************************************************************
 * Unless the stack is given by the user, we allocate a stack first and then
 * use the beginning of the allocated stack for allocating ABTI_thread and
 * ABTI_stack_header.  This way we need only one memory allocation call (e.g.,
 * ABTU_malloc).  The memory layout of the allocated stack will look like
 *  |-------------------|
 *  | ABTI_thread       |
 *  |-------------------|
 *  | ABTI_stack_header |
 *  |-------------------|
 *  | actual stack area |
 *  |-------------------|
 * Thus, the actual size of stack becomes
 * (requested stack size) - sizeof(ABTI_thread) - sizeof(ABTI_stack_header)
 * and it is set in the attribute field of ABTI_thread.
 *****************************************************************************/

static inline
ABTI_thread *ABTI_mem_alloc_thread()
{
    /* Basic idea: allocate a memory for stack and use the first some memory as
     * ABTI_stack_header and ABTI_thread. So, the effective stack area is
     * reduced as much as the size of ABTI_stack_header and ABTI_thread. */

    const size_t header_size = gp_ABTI_global->mem_sh_size;
    size_t stacksize;
    ABTI_local *p_local = lp_ABTI_local;
    char *p_blk = NULL;
    ABTI_thread *p_thread;
    ABTI_stack_header *p_sh;
    void *p_stack;

    /* Get the stack size */
    stacksize = F_STACK_SIZE + header_size;

    /* Use the stack pool */
    if (p_local->p_mem_stack) {
        /* ES's stack pool has an available stack */
        p_sh = p_local->p_mem_stack;
        p_local->p_mem_stack = p_sh->p_next;
        p_local->num_stacks--;

        p_sh->p_next = NULL;
        p_blk = (char *)p_sh - sizeof(ABTI_thread);

    } else {
        /* Check stacks in the global data */
        if (gp_ABTI_global->p_mem_stack) {
            p_blk = ABTI_mem_take_global_stack(p_local);
            if (p_blk == NULL) {
                p_blk = ABTI_mem_alloc_sp(p_local, stacksize);
            }
        } else {
            /* Allocate a new stack if we don't have any empty stack */
            p_blk = ABTI_mem_alloc_sp(p_local, stacksize);
        }

        p_sh = (ABTI_stack_header *)(p_blk + sizeof(ABTI_thread));
        p_sh->p_next = NULL;
    }

    /* Actual stack size */
    // actual_stacksize = stacksize - header_size;

    /* Get the ABTI_thread pointer and stack pointer */
    p_thread = (ABTI_thread *)p_blk;
    p_stack  = p_sh->p_stack;
    p_thread->stack = p_stack;
    return p_thread;
}

static inline
void ABTI_mem_free_thread(ABTI_thread *p_thread)
{
    ABTI_local *p_local;
    ABTI_stack_header *p_sh;

    p_sh = (ABTI_stack_header *)((char *)p_thread + sizeof(ABTI_thread));

    p_local = lp_ABTI_local;
    if (p_local->num_stacks <= gp_ABTI_global->mem_max_stacks) {
        p_sh->p_next = p_local->p_mem_stack;
        p_local->p_mem_stack = p_sh;
        p_local->num_stacks++;
    } else {
        ABTI_mem_add_stack_to_global(p_sh);
    }
}

#endif

#endif /* ABTI_MEM_H_INCLUDED */

