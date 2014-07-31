/*
 * PROJECT: GEMMapper
 * FILE: mm_stack.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 *     - StackMemory
 *         Memory allocator that serves memory in an stack fashion.
 *         All memory requested in successive calls to mm_stack_alloc() it's hold in memory.
 *         Once mm_stack_free() is called, all memory requests are freed (all at once).
 *         No individual memory free is possible with this type of memory.
 */

#include "mm_stack.h"

#define MM_STACK_INITIAL_SEGMENTS 10
#define MM_STACK_INITIAL_SEGMENTS_ALLOCATED 1

/*
 * Setup
 */
GEM_INLINE mm_stack_t* mm_stack_new(mm_slab_t* const mm_slab) {
  MM_SLAB_CHECK(mm_slab);
  // Allocate handler
  mm_stack_t* const mm_stack = mm_alloc(mm_stack_t);
  // Initialize slab & dimensions
  mm_stack->mm_slab = mm_slab;
  mm_stack->segment_size = mm_slab_get_slab_size(mm_slab);
  // Init segments
  mm_stack->segments = vector_new(MM_STACK_INITIAL_SEGMENTS,mm_stack_segment_t);
  vector_set_used(mm_stack->segments,MM_STACK_INITIAL_SEGMENTS_ALLOCATED);
  VECTOR_ITERATE(mm_stack->segments,stack_segment,position,mm_stack_segment_t) {
    stack_segment->slab_unit = mm_slab_request(mm_slab);
    stack_segment->memory = stack_segment->slab_unit->memory;
    stack_segment->memory_available = mm_stack->segment_size;
  }
  // Return
  return mm_stack;
}
GEM_INLINE void mm_stack_delete(mm_stack_t* const mm_stack) {
  MM_STACK_CHECK(mm_stack);
  // Return all slabs
  mm_slab_lock(mm_stack->mm_slab);
  VECTOR_ITERATE(mm_stack->segments,stack_segment,position,mm_stack_segment_t) {
    mm_slab_put(mm_stack->mm_slab,stack_segment->slab_unit);
  }
  mm_slab_unlock(mm_stack->mm_slab);
  // Free segments' vector
  vector_delete(mm_stack->segments);
  // Free handler
  mm_free(mm_stack);
}

/*
 * Accessors
 */
GEM_INLINE mm_stack_segment_t* mm_stack_add_segment(mm_stack_t* const mm_stack) {
  // Add new segment
  vector_reserve_additional(mm_stack->segments,1);
  vector_inc_used(mm_stack->segments);
  mm_stack_segment_t* const stack_segment = vector_get_last_elm(mm_stack->segments,mm_stack_segment_t);
  // Init segment
  stack_segment->slab_unit = mm_slab_request(mm_stack->mm_slab);
  stack_segment->memory = stack_segment->slab_unit->memory;
  stack_segment->memory_available = mm_stack->segment_size;
  // Return segment
  return stack_segment;
}
GEM_INLINE void* mm_stack_memory_allocate(mm_stack_t* const mm_stack,const uint64_t num_bytes,const bool zero_mem) {
  MM_STACK_CHECK(mm_stack);
  // Get last stack segment
  mm_stack_segment_t* mm_stack_segment = vector_get_last_elm(mm_stack->segments,mm_stack_segment_t);
  // Check if there is enough free memory in the segment
  if (gem_expect_false(num_bytes > mm_stack_segment->memory_available)) {
    // Check we can fit the request into a slab unit
    gem_cond_fatal_error(num_bytes > mm_stack->segment_size,
        MM_STACK_CANNOT_ALLOCATE,num_bytes,mm_stack->segment_size);
    // Add new segment
    mm_stack_segment = mm_stack_add_segment(mm_stack);
  }
  // Serve Memory Request
  mm_stack_segment->memory_available -= num_bytes;
  void* const memory = mm_stack_segment->memory;
  mm_stack_segment->memory += num_bytes;
  // Check zero mem
  if (gem_expect_false(zero_mem)) memset(memory,0,num_bytes);
  // Return memory
  return memory;
}
GEM_INLINE void mm_stack_free(mm_stack_t* const mm_stack) {
  MM_STACK_CHECK(mm_stack);
  // Clear first Segment
  mm_stack_segment_t* const first_stack_segment = vector_get_elm(mm_stack->segments,0,mm_stack_segment_t);
  first_stack_segment->memory = first_stack_segment->slab_unit->memory;
  first_stack_segment->memory_available = mm_stack->segment_size;
  // Reap non-resident segments
  mm_slab_lock(mm_stack->mm_slab);
  VECTOR_OFFSET_ITERATE(mm_stack->segments,stack_segment,position,1,mm_stack_segment_t) {
    mm_slab_put(mm_stack->mm_slab,stack_segment->slab_unit);
  }
  mm_slab_unlock(mm_stack->mm_slab);
  // Set used to 1
  vector_set_used(mm_stack->segments,1);
}
