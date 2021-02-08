// R. Jesse Chaney
// chaneyr@oregonstate.edu

#include "beavalloc.h"

typedef struct mem_block_s {
    uint8_t free;

    size_t capacity;
    size_t size;

    struct mem_block_s *prev;
    struct mem_block_s *next;
} mem_block_t;

#define BLOCK_SIZE (sizeof(mem_block_t))
#define BLOCK_DATA(__curr) (((void *) __curr) + (BLOCK_SIZE))

static mem_block_t *block_list_head = NULL;
static mem_block_t *block_list_tail = NULL;

static void *lower_mem_bound = NULL;
static void *upper_mem_bound = NULL;

static uint8_t isVerbose = FALSE;
static FILE *beavalloc_log_stream = NULL;

// This is some gcc magic.
static void init_streams(void) __attribute__((constructor));

// Leave this alone
static void 
init_streams(void)
{
    beavalloc_log_stream = stderr;
}

// Leave this alone
void 
beavalloc_set_verbose(uint8_t verbosity)
{
    isVerbose = verbosity;
    if (isVerbose) {
        fprintf(beavalloc_log_stream, "Verbose enabled\n");
    }
}

// Leave this alone
void 
beavalloc_set_log(FILE *stream)
{
    beavalloc_log_stream = stream;
}

void *
beavalloc(size_t size)
{
    void *ptr = NULL;
    int capacity = 0;
    struct mem_block_s *new_block = NULL;
    struct mem_block_s *block_parse = NULL;
    
    
    if(size == 0){
      return NULL;
    }
    
    
    //If this is the first allocation
    if(block_list_head == NULL){
    
      //Set capacity to nearest multiple of 1024
      capacity = (int)((size + BLOCK_SIZE + 1023) / 1024);
      capacity *= 1024;
    
      //sbrk to request new memory
      ptr = sbrk(capacity);
      if(ptr == (void *)-1){
        return NULL;
      }
      
      //set higher and lower bounds
      upper_mem_bound = ptr + capacity;
      lower_mem_bound = ptr;
      
      //set new block values
      new_block = ptr;
      new_block->size = size;
      new_block->capacity = capacity-BLOCK_SIZE;
      new_block->prev = NULL;
      new_block->next = NULL;
      new_block->free = 0;
    
      //set block list head to new block
      block_list_head = new_block;
      block_list_tail = new_block;
      
      ptr = BLOCK_DATA(new_block);
      return ptr;
      
    } else {
    
      //Check for blocks to split
      block_parse = block_list_head;
      while(block_parse != NULL){
      
        //If there is enough room
        if(block_parse->capacity - block_parse->size >= size + BLOCK_SIZE){
        
          if(block_parse->free > 0){
              block_parse->size = size;
              block_parse->free = 0;
              return BLOCK_DATA(block_parse);
          }
          
          //Set new block to be the end of parsed block
          new_block = (void*)block_parse + BLOCK_SIZE + block_parse->size;
          
          new_block->size = size;
          new_block->capacity = block_parse->capacity - BLOCK_SIZE - block_parse->size;
                    
          new_block->prev = block_parse;
          new_block->next = block_parse->next;
          new_block->free = 0;
          
          if(new_block->next != NULL){
            new_block->next->prev = new_block;
          }
          
          block_parse->next = new_block;
          block_parse->capacity = block_parse->size;
          
          if(block_list_tail == block_parse){
            block_list_tail = new_block;
          }
          
          
          ptr = BLOCK_DATA(new_block);
          return ptr;
        }
        else {
          block_parse = block_parse->next;
        }
      }
    
      //Set capacity to nearest multiple of 1024
      capacity = (int)((size + BLOCK_SIZE + 1023) / 1024);
      capacity *= 1024;
    
      //sbrk to request new memory
      ptr = sbrk(capacity);
      if(ptr == (void *)-1){
        return NULL;
      }
      
      //set higher bounds
      upper_mem_bound = ptr + capacity;
      
      //Set new block values
      new_block = ptr;
      new_block->size = size;
      new_block->capacity = capacity-BLOCK_SIZE;
      new_block->prev = block_list_tail;
      new_block->next = NULL;
      new_block->free = 0;
      
      //Set previous block's next
      block_list_tail->next = new_block;
    
      //set block list head to new block
      block_list_tail = new_block;
      
      ptr = BLOCK_DATA(new_block);
      return ptr;
    
    }
    
    return ptr;
}

void 
beavfree(void *ptr)
{
    
    struct mem_block_s *mem_block = NULL;
    struct mem_block_s *prev_block = NULL;

    if(ptr == NULL){
      return;
    }
    
    mem_block = ptr-BLOCK_SIZE;
    
    if(mem_block->free > 0){
      if(isVerbose){
        fprintf(beavalloc_log_stream, "You tried to free a free free: %p\n", mem_block);
      }
      return;
    }
    
    //Free block
    mem_block->size = 0;
    mem_block->free = 1;
    
    //if the next block is free
    if(mem_block->next != NULL && mem_block->next->free > 0){
      //add next block to capacity
      mem_block->capacity += BLOCK_SIZE + mem_block->next->capacity;
      //set next value to the next->next
      mem_block->next = mem_block->next->next;
      //set new next value's prev to this block
      if(mem_block->next != NULL){
        mem_block->next->prev = mem_block;
      }
      mem_block->free = 0;
      beavfree(BLOCK_DATA(mem_block));
    } else if(mem_block->prev != NULL && mem_block->prev->free > 0){
      //same thing but in reverse
      prev_block = mem_block->prev;
      //add next block to capacity
      prev_block->capacity += BLOCK_SIZE + mem_block->capacity;
      //set next value to the next->next
      prev_block->next = mem_block->next;
      
      if(mem_block->next != NULL){
        //set new next value's prev to this block
        mem_block->next->prev = prev_block;
      }
      prev_block->free = 0;
      beavfree(BLOCK_DATA(prev_block));
    }
    
    
    return;
}


void 
beavalloc_reset(void)
{
  if(lower_mem_bound != NULL){
    brk(lower_mem_bound);
  }
  
  block_list_head = NULL;
  block_list_tail = NULL;
  
  return;
}

void *
beavcalloc(size_t nmemb, size_t size)
{
    void *ptr = NULL;
    
    if(nmemb == 0 || size == 0){
      return ptr;
    }
    ptr = beavalloc(nmemb*size);
    if(ptr != NULL){
      memset(ptr, 0, nmemb*size);
    }

    return ptr;
}

void *
beavrealloc(void *ptr, size_t size)
{
    void *nptr = NULL;
    struct mem_block_s *mem_block = NULL;
    
    if(ptr == NULL){
      return beavalloc(size);
    }
    
    if(size == 0){
      beavfree(ptr);
      return NULL;
    }    
    
    mem_block = ptr - BLOCK_SIZE;
    
    if(mem_block->capacity > size){
      mem_block->size = size;
      return BLOCK_DATA(mem_block);
    }
    
    beavfree(BLOCK_DATA(mem_block));
    nptr = beavalloc(size);
    
    if(nptr != ptr){
      memcpy(nptr, ptr, size);
    }
    

    return nptr;
}

void *
beavstrdup(const char *s)
{
    void *nptr = NULL;
    
    size_t length = strlen(s);
    size_t size = length*sizeof(char) + 1;
    
    if(size > 0){
      nptr = beavalloc(size);
      memcpy(nptr, s, size);
    }

    return nptr;
}

// Leave this alone
void 
beavalloc_dump(void)
{
    mem_block_t *curr = NULL;
    unsigned i = 0;
    unsigned user_bytes = 0;
    unsigned capacity_bytes = 0;
    unsigned block_bytes = 0;
    unsigned used_blocks = 0;
    unsigned free_blocks = 0;

    fprintf(beavalloc_log_stream, "Heap map\n");
    fprintf(beavalloc_log_stream
            , "  %s\t%s\t%s\t%s\t%s" 
              "\t%s\t%s\t%s\t%s\t%s"
            "\n"
            , "blk no  "
            , "block add "
            , "next add  "
            , "prev add  "
            , "data add  "
            
            , "blk size "
            , "capacity "
            , "size     "
            , "excess   "
            , "status   "
        );
    for (curr = block_list_head, i = 0; curr != NULL; curr = curr->next, i++) {
        fprintf(beavalloc_log_stream
                , "  %u\t\t%9p\t%9p\t%9p\t%9p\t%u\t\t%u\t\t"
                  "%u\t\t%u\t\t%s\t%c"
                , i
                , curr
                , curr->next
                , curr->prev
                , BLOCK_DATA(curr)

                , (unsigned) (curr->capacity + BLOCK_SIZE)
                , (unsigned) curr->capacity
                , (unsigned) curr->size
                , (unsigned) (curr->capacity - curr->size)
                , curr->free ? "free  " : "in use"
                , curr->free ? '*' : ' '
            );
        fprintf(beavalloc_log_stream, "\n");
        user_bytes += curr->size;
        capacity_bytes += curr->capacity;
        block_bytes += curr->capacity + BLOCK_SIZE;
        if (curr->free == TRUE) {
            free_blocks++;
        }
        else {
            used_blocks++;
        }
    }
    fprintf(beavalloc_log_stream
            , "  %s\t\t\t\t\t\t\t\t"
              "%u\t\t%u\t\t%u\t\t%u\n"
            , "Total bytes used"
            , block_bytes
            , capacity_bytes
            , user_bytes
            , capacity_bytes - user_bytes
        );
    fprintf(beavalloc_log_stream
            , "  Used blocks: %4u  Free blocks: %4u  "
              "Min heap: %9p    Max heap: %9p   Block size: %lu bytes\n"
            , used_blocks
            , free_blocks
            , lower_mem_bound
            , upper_mem_bound
            , BLOCK_SIZE
        );
}
