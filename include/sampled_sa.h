/*
 * PROJECT: GEMMapper
 * FILE: sampled_sa.h
 * DATE: 06/06/2013
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 */

#ifndef SAMPLED_SA_H_
#define SAMPLED_SA_H_

#include "essentials.h"
#include "packed_integer_array.h"

//#define SAMPLING_SA_DIRECT  // Text-sampling
#define SAMPLING_SA_INVERSE // SA-sampling

typedef enum { SAMPLING_RATE_1=0,  SAMPLING_RATE_2=1,   SAMPLING_RATE_4=2,
               SAMPLING_RATE_8=3,  SAMPLING_RATE_16=4,  SAMPLING_RATE_32=5,
               SAMPLING_RATE_64=6, SAMPLING_RATE_128=7, SAMPLING_RATE_256=8 } sampling_rate_t;

/*
 * Sampled SA
 */
typedef struct {
  // Meta-data
  uint64_t index_length;
  sampling_rate_t sampling_rate;
  // Integer Array
  packed_integer_array_t* packed_integer_array;
} sampled_sa_t;
/*
 * Sampled SA Builder
 */
typedef struct {
  // Meta-data
  uint64_t index_length;
  sampling_rate_t sampling_rate;
  // Integer Array Builder
  uint64_t num_chunks;
  packed_integer_array_builder_t** array_builder;
#ifdef SAMPLING_SA_DIRECT
  // Sampled Positions Bitmap
  pthread_mutex_t sampled_bitmap_mutex;
  uint64_t sampled_bitmap_size;
  uint64_t* sampled_bitmap_mem;
#endif
} sampled_sa_builder_t;

/*
 * Loader
 */
GEM_INLINE sampled_sa_t* sampled_sa_new(const uint64_t index_length,const sampling_rate_t sampling_rate);
GEM_INLINE sampled_sa_t* sampled_sa_read(fm_t* const file_manager);
GEM_INLINE sampled_sa_t* sampled_sa_read_mem(mm_t* const memory_manager);
GEM_INLINE void sampled_sa_write(fm_t* const file_manager,sampled_sa_t* const sampled_sa);
GEM_INLINE void sampled_sa_delete(sampled_sa_t* const sampled_sa);

/*
 * Builder
 */
GEM_INLINE sampled_sa_builder_t* sampled_sa_builder_new(
    const uint64_t index_length,const sampling_rate_t sampling_rate,
    const uint64_t num_builders,mm_slab_t* const mm_slab);
GEM_INLINE void sampled_sa_builder_delete_samples(sampled_sa_builder_t* const sampled_sa);
GEM_INLINE void sampled_sa_builder_delete(sampled_sa_builder_t* const sampled_sa);

GEM_INLINE void sampled_sa_builder_set_sample(
    sampled_sa_builder_t* const sampled_sa,const uint64_t chunk_number,
    const uint64_t array_position,const uint64_t sa_value);
GEM_INLINE void sampled_sa_builder_write(fm_t* const file_manager,sampled_sa_builder_t* const sampled_sa);

GEM_INLINE uint64_t sampled_sa_builder_get_sampling_rate(const sampled_sa_builder_t* const sampled_sa);
GEM_INLINE uint64_t* sampled_sa_builder_get_sampled_bitmap(const sampled_sa_builder_t* const sampled_sa);

/*
 * Accessors
 */
GEM_INLINE uint64_t sampled_sa_get_size(const sampled_sa_t* const sampled_sa);
GEM_INLINE uint64_t sampled_sa_get_sampling_rate(const sampled_sa_t* const sampled_sa);

GEM_INLINE void sampled_sa_prefetch_sample(const sampled_sa_t* const sampled_sa,const uint64_t array_position);
GEM_INLINE uint64_t sampled_sa_get_sample(const sampled_sa_t* const sampled_sa,const uint64_t array_position);
GEM_INLINE void sampled_sa_set_sample(sampled_sa_t* const sampled_sa,const uint64_t array_position,const uint64_t sa_value);

/*
 * Display/Stats
 */
GEM_INLINE void sampled_sa_print(FILE* const stream,sampled_sa_t* const sampled_sa,const bool display_data);
GEM_INLINE void sampled_sa_builder_print(FILE* const stream,sampled_sa_builder_t* const sampled_sa);

#endif /* SAMPLED_SA_H_ */
