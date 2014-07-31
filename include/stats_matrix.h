/*
 * PROJECT: GEMMapper
 * FILE: stats_matrix.h
 * DATE: 07/06/2013
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: TODO
 */

#ifndef STATS_MATRIX_H_
#define STATS_MATRIX_H_

#include "essentials.h"
#include "stats_vector.h"

/*
 * Checkers
 */
#define STATS_MATRIX_CHECK(stats_matrix) \
  GEM_CHECK_NULL(stats_matrix)

/*
 * Stats Matrix f(x,y) := (StatsVector x StatsVector)
 */
typedef struct {
  stats_vector_t* dimension_x; // x dimension
  stats_vector_t* dimension_y; // Template for the y dimension
} stats_matrix_t;

/*
 * Setup
 */
GEM_INLINE stats_matrix_t* stats_matrix_new(stats_vector_t* const dimension_x,stats_vector_t* const dimension_y);
GEM_INLINE void stats_matrix_clear(stats_matrix_t* const stats_matrix);
GEM_INLINE void stats_matrix_delete(stats_matrix_t* const stats_matrix);

/*
 * Increment/Add bucket counter
 */
GEM_INLINE void stats_matrix_inc(stats_matrix_t* const stats_matrix,const uint64_t value_x,const uint64_t value_y);
GEM_INLINE void stats_matrix_add(stats_matrix_t* const stats_matrix,const uint64_t value_x,const uint64_t value_y,const uint64_t amount);

/*
 * Bucket counters getters (Individual buckets)
 */
GEM_INLINE uint64_t stats_matrix_get_count(stats_matrix_t* const stats_matrix,const uint64_t value_x,const uint64_t value_y);

/*
 * Display (Printers)
 */
GEM_INLINE void stats_matrix_display(
    FILE* const stream,stats_matrix_t* const stats_matrix,
    const bool display_percentage,void (*print_label)(uint64_t));

/*
 * Errors
 */
//#define GEM_ERROR_STATS_MATRIX_ ""

#endif /* STATS_MATRIX_H_ */