/*
 * PROJECT: GEMMapper
 * FILE: vector.h
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: Simple linear vector for generic type elements
 *
 * COMPILATION_FLAGS:
 *   GEM_DEBUG
 */

#ifndef VECTOR_H_
#define VECTOR_H_

#include "commons.h"

/*
 * Checkers
 */
#define VECTOR_CHECK(vector) \
  GEM_CHECK_NULL(vector); \
  GEM_CHECK_NULL(vector->memory); \
  GEM_CHECK_ZERO(vector->element_size)
#define VECTOR_RANGE_CHECK(vector,position) \
  VECTOR_CHECK(vector); \
  gem_fatal_check(position>=(vector)->used||position<0,POSITION_OUT_OF_RANGE, \
      (uint64_t)position,(uint64_t)0,((int64_t)(vector)->used)-1);
/*
 * Data Structures
 */
typedef struct {
  void* memory;
  uint64_t used;
  uint64_t element_size;
  uint64_t elements_allocated;
} vector_t;

/*
 * Vector Setup (Initialization & Allocation)
 */
#define vector_new(num_initial_elements,type) vector_new_(num_initial_elements,sizeof(type))
GEM_INLINE vector_t* vector_new_(const uint64_t num_initial_elements,const uint64_t element_size);
GEM_INLINE void vector_reserve(vector_t* const vector,const uint64_t num_elements,const bool zero_mem);
GEM_INLINE void vector_resize__clear(vector_t* const vector,const uint64_t num_elements);
#define vector_cast__clear(vector,type) vector_cast__clear_s(vector,sizeof(type))
GEM_INLINE void vector_cast__clear_(vector_t* const vector,const uint64_t element_size);
#define vector_clear(vector) (vector)->used=0
GEM_INLINE void vector_delete(vector_t* const vector);
#define vector_is_empty(vector) (vector_get_used(vector)==0)
#define vector_reserve_additional(vector,additional) vector_reserve(vector,vector_get_used(vector)+additional,false)
#define vector_prepare(vector,num_elements,type) \
  vector_cast__clear(vector,sizeof(type)); \
  vector_reserve(vector,num_elements,false);

/*
 * Element Getters/Setters
 */
#define vector_get_mem(vector,type) ((type*)((vector)->memory))
#define vector_get_last_elm(vector,type) (vector_get_mem(vector,type)+(vector)->used-1)
#define vector_get_free_elm(vector,type) (vector_get_mem(vector,type)+(vector)->used)
#ifndef GEM_DEBUG
  #define vector_get_elm(vector,position,type) (vector_get_mem(vector,type)+position)
#else
  GEM_INLINE void* vector_get_mem_element(vector_t* const vector,const uint64_t position,const uint64_t element_size);
  #define vector_get_elm(vector,position,type) ((type*)vector_get_mem_element(vector,position,sizeof(type)))
#endif
#define vector_set_elm(vector,position,type,elm) *vector_get_elm(vector,position,type) = elm

/*
 * Used elements Getters/Setters
 */
#define vector_get_used(vector) ((vector)->used)
#define vector_set_used(vector,total_used) (vector)->used=(total_used)
#define vector_inc_used(vector) (++((vector)->used))
#define vector_dec_used(vector) (--((vector)->used))
#define vector_add_used(vector,additional) vector_set_used(vector,vector_get_used(vector)+additional)
#define vector_update_used(vector,pointer_to_next_free_element) \
  (vector)->used = (pointer_to_next_free_element) - vector_get_mem(vector,__typeof__(pointer_to_next_free_element))


/*
 * Vector Allocate/Insert (Get a new element or Add an element to the end of the vector)
 */
#define vector_alloc_new(vector,type,return_element_pointer) { \
  vector_reserve_additional(vector,1); \
  return_element_pointer = vector_get_free_elm(vector,type); \
  vector_inc_used(vector); \
}
#define vector_insert(vector,element,type) { \
  vector_reserve_additional(vector,1); \
  *(vector_get_free_elm(vector,type))=element; \
  vector_inc_used(vector); \
}

/*
 * Macro generic iterator
 *   VECTOR_ITERATE(vector_of_ints,elm_iterator,elm_counter,int) {
 *     ..code..
 *   }
 */
#define VECTOR_ITERATE(vector,element,counter,type) \
  uint64_t counter; \
  type* element = vector_get_mem(vector,type); \
  for (counter=0;counter<vector_get_used(vector);++element,++counter)
#define VECTOR_OFFSET_ITERATE(vector,element,counter,offset,type) \
  uint64_t counter; \
  type* element = vector_get_elm(vector,offset,type); \
  for (counter=offset;counter<vector_get_used(vector);++element,++counter)

/*
 * Miscellaneous
 */
GEM_INLINE void vector_copy(vector_t* const vector_to,vector_t* const vector_from);
GEM_INLINE vector_t* vector_dup(vector_t* const vector_src);

/*
 * Error Messages
 */
#define GEM_ERROR_VECTOR_NEW "Could not create new vector (%"PRIu64" bytes requested)"
#define GEM_ERROR_VECTOR_RESERVE "Could not reserve vector (%"PRIu64" bytes requested)"

#endif /* VECTOR_H_ */