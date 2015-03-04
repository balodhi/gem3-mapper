/*
 * PROJECT: GEMMapper
 * FILE: region_profile.h
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 */

#ifndef REGION_PROFILE_H_
#define REGION_PROFILE_H_

#include "essentials.h"

#include "fm_index.h"
#include "pattern.h"

#include "stats_vector.h"
#include "stats_matrix.h"

// Degree to filter region
#define REGION_FILTER_NONE 0
#define REGION_FILTER_DEGREE_ZERO 1
#define REGION_FILTER_DEGREE_ONE  2
#define REGION_FILTER_DEGREE_TWO  3

/*
 * Region Type:
 *   - region_gap: Constitutes a gap in the region profile (Eg. Contains wildcards).
 *   - region_standard: Region with some candidates. Should only be filtered up to 0-misms
 *   - region_unique: Very few candidates or none. Can be filtered up to n-errors (n>=0)
 */
typedef enum { region_unique, region_standard, region_gap } region_type;
typedef struct {
  uint64_t region_th;      // Max. number of candidates allowed per region
  uint64_t max_steps;      // Max. number of characters to explore to improve the region
  uint64_t dec_factor;     // Decreasing factor per step in region exploration
  uint64_t region_type_th; // Threshold to classify regions {ZERO,NON_ZERO}
} region_profile_model_t;
// Filtering regions
typedef struct {
  // Ranges of the region [end,start) (Yes!, backwards like the FM-search)
  uint64_t start;
  uint64_t end;
  region_type type;
  // Mismatches required to match the region
  uint64_t max;
  uint64_t min;
  // Region filtering results
  uint64_t hi;
  uint64_t lo;
  // Degree assigned to this region
  uint64_t degree;
} region_search_t;
// Region Profile
typedef struct {
  uint64_t id;
  uint64_t value;
} region_locator_t;
typedef struct {
  /* Pattern */
  uint64_t pattern_length;
  /* Filtering regions */
  region_search_t* filtering_region; // Filtering regions
  uint64_t num_filtering_regions;    // Total number of filtering regions (region_unique + region_standard + region_gap)
  uint64_t num_standard_regions;     // Number of Standard Regions
  /* Mismatch regions */
  region_search_t* search_region;    // Search regions
  uint64_t num_search_regions;
  /* Region Partition Properties */
  uint64_t errors_allowed;           // Total error allowed (minimum required to get a novel match)
  /* Locator for region sorting */
  region_locator_t* loc;
} region_profile_t;

/*
 * Setup
 */
GEM_INLINE void region_profile_new(
    region_profile_t* const region_profile,const uint64_t pattern_length,
    mm_stack_t* const mm_stack);

/*
 * Accessors
 */
GEM_INLINE uint64_t region_get_num_regions(region_profile_t* const region_profile);
GEM_INLINE bool region_profile_has_exact_matches(region_profile_t* const region_profile);

/*
 * Region Profile Generation
 */
GEM_INLINE void region_profile_generate_fixed(
    region_profile_t* const region_profile,fm_index_t* const fm_index,
    pattern_t* const pattern,const bool* const allowed_enc,
    const region_profile_model_t* const profile_model,const uint64_t min_regions);
GEM_INLINE void region_profile_generate_adaptive(
    region_profile_t* const region_profile,fm_index_t* const fm_index,
    pattern_t* const pattern,const bool* const allowed_enc,
    const region_profile_model_t* const profile_model,
    const uint64_t max_regions,const bool allow_zero_regions);
GEM_INLINE void region_profile_generate_full_progressive(
    region_profile_t* const region_profile,
    region_search_t* const base_region,const uint64_t start_region,const uint64_t total_regions);

/*
 * Region Profile Utils
 */
GEM_INLINE void region_profile_sort_by_estimated_mappability(region_profile_t* const region_profile);
GEM_INLINE void region_profile_sort_by_candidates(region_profile_t* const region_profile);
GEM_INLINE void region_profile_fill_gaps(
    region_profile_t* const region_profile,const uint64_t eff_mismatches);

/*
 * Display
 */
GEM_INLINE void region_profile_print(
    FILE* const stream,const region_profile_t* const region_profile,
    const bool sorted,const bool display_misms_regions);

/*
 * Iterator
 */
#define REGION_PROFILE_ITERATE(region_profile,region,position) \
  const uint64_t num_filtering_regions = region_profile->num_filtering_regions; \
  region_search_t* region = region_profile->filtering_region; \
  uint64_t position; \
  for (position=0;position<num_filtering_regions;++position,++region)
#define REGION_LOCATOR_ITERATE(region_profile,region,position) \
  const uint64_t num_filtering_regions = region_profile->num_filtering_regions; \
  region_search_t* const filtering_region = region_profile->filtering_region; \
  region_locator_t* const loc = region_profile->loc; \
  region_search_t* region; \
  uint64_t position; \
  for (position=0,region=filtering_region+loc[0].id; \
       position<num_filtering_regions; \
       ++position,region=filtering_region+loc[position].id)

#endif /* REGION_PROFILE_H_ */
