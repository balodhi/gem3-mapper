/*
 * PROJECT: GEMMapper
 * FILE: mm_slab.h
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 *     - SlabMemory
 *         Relative big amounts of contiguous memory allocated all at once (like the LINUX slab allocator)
 *         Objects of a certain type are ready to go inside the slab reducing
 *         the overhead of malloc/setup/free cycles along the program
 *         Offers thread safe allocation of slabs as to balance memory consumption across threads
 */

#ifndef MM_SLAB_H_
#define MM_SLAB_H_

#include "commons.h"
#include "errors.h"
#include "vector.h"
#include "mm.h"

#define MM_UNLIMITED_MEM UINT64_MAX

typedef struct _mm_slab_segment_t mm_slab_segment_t; // Forward declaration of mm_slab_segment_t
typedef struct {
  void* memory;                     /* Memory chunk of PageSize Bytes */
  mm_slab_segment_t* slab_segment;  /* Slab segment associated */
} mm_slab_unit_t;
struct _mm_slab_segment_t {
  uint64_t segment_id;
  mm_t* mm;                    /* Memory manager */
  mm_slab_unit_t* slab_units;  /* Slab units of the segment */
  uint64_t total_slabs_units;  /* Total slab units of the segment */
  uint64_t busy_slabs_units;   /* Used slab units of the segment */
};
typedef struct {
  /* Slab */
  uint64_t slab_id;            /* Slab ID */
  char* description;           /* Slab Description */
  uint64_t max_memory;         /* Maximum memory allocatable before going to disk (tmp files) */
  uint64_t requested_memory;   /* Total memory requested by the slab */
  /* Slab Segments */
  uint64_t slab_segment_size;  /* Bytes per slab segment */
  vector_t* slabs_segments;    /* (mm_slab_segment*) */
  /* Slab Units */
  uint64_t slab_unit_size;     /* Bytes per slab unit */
  vector_t* slabs_units_free;  /* Free slab units (mm_slab_unit*) */
  /* Mutex */
  pthread_mutex_t slab_mutex;
  /* Internal */
  uint64_t segment_id_generator;
} mm_slab_t;

/*
 * Checkers
 */
#define MM_SLAB_CHECK(mm_slab) \
  VECTOR_CHECK((mm_slab)->slabs_segments); \
  VECTOR_CHECK((mm_slab)->slabs_units_free)
#define MM_SLAB_SEGMENT_CHECK(mm_slab_segment) \
  MM_CHECK((mm_slab_segment)->mm)
#define MM_SLAB_UNIT_CHECK(mm_slab_unit) \
  MM_SLAB_SEGMENT_CHECK((mm_slab_unit)->slab_segment); \
  GEM_CHECK_NULL((mm_slab_unit)->memory)

#define MM_SLAB_SEGMENT_INITIAL_SIZE BUFFER_SIZE_32M

/*
 * Setup
 */
#define mm_slab_new(slab_size) mm_slab_new_(slab_size,MM_SLAB_SEGMENT_INITIAL_SIZE,MM_UNLIMITED_MEM,"")
GEM_INLINE mm_slab_t* mm_slab_new_(
    const uint64_t slab_size,const uint64_t slab_segment_size,
    const uint64_t max_allocatable_memory,char* const description);
GEM_INLINE void mm_slab_reap_empty(mm_slab_t* const mm_slab,const uint64_t num_resident_segments);
GEM_INLINE void mm_slab_delete(mm_slab_t* const mm_slab);

/*
 *  Accessors
 */
GEM_INLINE void mm_slab_lock(mm_slab_t* const mm_slab);
GEM_INLINE void mm_slab_unlock(mm_slab_t* const mm_slab);
GEM_INLINE mm_slab_unit_t* mm_slab_get(mm_slab_t* const mm_slab);
GEM_INLINE void mm_slab_put(mm_slab_t* const mm_slab,mm_slab_unit_t* const mm_slab_unit);

/*
 *  Accessors (ThreadSafe)
 */
GEM_INLINE mm_slab_unit_t* mm_slab_request(mm_slab_t* const mm_slab);
GEM_INLINE void mm_slab_return(mm_slab_t* const mm_slab,mm_slab_unit_t* const mm_slab_unit);
GEM_INLINE uint64_t mm_slab_get_slab_size(mm_slab_t* const mm_slab);

/*
 * Defragment Slab
 */
GEM_INLINE void mm_slab_defragment(mm_slab_t* const mm_slab);

/*
 * Display/Profile
 */
GEM_INLINE void mm_slab_print(FILE* const stream,mm_slab_t* const mm_slab,const bool show_internals);

/*
 * Error Messages
 */
#define GEM_ERROR_MM_SLAB_WRONG_DIMENSIONS "Wrong Dimensions. Slab-Segment size (%"PRIu64") must be non-zero and (>=) than the size of each Slab-Unit (%"PRIu64")"
#define GEM_ERROR_MM_SLAB_WASTED_MEM "Slab Allocated Memory is wasted. Slab-Segment size (%"PRIu64") is not a multiple of the size of each Slab-Unit (%"PRIu64")"

#endif /* MM_SLAB_H_ */