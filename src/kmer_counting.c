/*
 * PROJECT: GEMMapper
 * FILE: kmer_counting.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 *
 * TODO
 *   Trick:
 *     Check for Ns, if it doesn't have N's then make a much simpler check of the candidate read
 *
 *   Title: "Verification tests"
 *     Can be heuristic, like discarding a candidate region if
 *     if doesn't have enough seed supporting it. Can be exact like k-mer counting.
 *     Both can give an upper/lower bound on the matching distance.
 *       Seed-counting => At least 1 error per seed not matching (it needs at least
 *                        MismsRegions to make a match)
 *       K-mer counting => ...
 *
 *   Title: "Sorting candidate regions towards aligning only the best"
 *
 */

#include "kmer_counting.h"
#include "bpm_align.h"

/*
 * Constants
 */
//#define KMER_COUNTING_LENGTH 3
//#define KMER_COUNTING_MASK   0x000000000000003Full
//#define KMER_COUNTING_LENGTH 4
//#define KMER_COUNTING_MASK   0x00000000000000FFull
#define KMER_COUNTING_LENGTH 5
#define KMER_COUNTING_MASK   0x00000000000003FFull
//#define KMER_COUNTING_LENGTH 6
//#define KMER_COUNTING_MASK   0x0000000000000FFFull

#define KMER_COUNTING_NUM_KMERS POW4(KMER_COUNTING_LENGTH)
#define KMER_COUNTING_MASK_INDEX(kmer_idx) ((kmer_idx) & KMER_COUNTING_MASK)
#define KMER_COUNTING_ADD_INDEX(kmer_idx,enc_char) kmer_idx = (kmer_idx<<2 | (enc_char))
#define KMER_COUNTING_ADD_INDEX__MASK(kmer_idx,enc_char) kmer_idx = KMER_COUNTING_MASK_INDEX(kmer_idx<<2 | (enc_char))

#define KMER_COUNTING_EFFECTIVE_THRESHOLD 12

/*
 * Compile Pattern
 */
GEM_INLINE void kmer_counting_compile(
    kmer_counting_t* const kmer_counting,uint8_t* const pattern,
    const uint64_t pattern_length,const uint64_t num_non_canonical_bases,
    const uint64_t effective_filtering_max_error,mm_stack_t* const mm_stack) {
  // Check efficiency conditions (if not meet, the filter is disabled)
  if (num_non_canonical_bases>0 || pattern_length < KMER_COUNTING_LENGTH ||
      pattern_length/effective_filtering_max_error < KMER_COUNTING_EFFECTIVE_THRESHOLD) {
    kmer_counting->enabled = false;
    return;
  }
  // Init
  kmer_counting->enabled = true;
  kmer_counting->kmer_count_text = mm_stack_calloc(mm_stack,KMER_COUNTING_NUM_KMERS,uint16_t,true);
  kmer_counting->kmer_count_pattern = mm_stack_calloc(mm_stack,KMER_COUNTING_NUM_KMERS,uint16_t,true);
  kmer_counting->pattern_length = pattern_length;
  // Count kmers in pattern
  uint64_t pos, kmer_idx;
  // Initial Fill
  for (pos=0,kmer_idx=0;pos<KMER_COUNTING_LENGTH-1;++pos) {
    const uint8_t enc_char = pattern[pos];
    KMER_COUNTING_ADD_INDEX__MASK(kmer_idx,enc_char);  // Update kmer-index
  }
  // Compile all k-mers
  for (;pos<pattern_length;++pos) {
    const uint8_t enc_char = pattern[pos];
    KMER_COUNTING_ADD_INDEX__MASK(kmer_idx,enc_char);  // Update kmer-index
    ++(kmer_counting->kmer_count_pattern[kmer_idx]);   // Increment kmer-count
  }
}
/*
 * Filter text region
 */
GEM_INLINE uint64_t kmer_counting_filter(
    const kmer_counting_t* const kmer_counting,const uint8_t* const text,
    const uint64_t text_length,const uint64_t max_error) {
  // Check filter enabled
  if (!kmer_counting->enabled) return 0; // Don't filter
  // Check filter usable
  const uint64_t kmers_error = KMER_COUNTING_LENGTH*max_error;
  const uint64_t kmers_max = kmer_counting->pattern_length - (KMER_COUNTING_LENGTH-1);
  if (kmers_error >= kmers_max) return 0; // Don't filter
  PROF_START(GP_FC_KMER_COUNTER_FILTER);
  // Prepare filter
  const uint64_t kmers_required = kmer_counting->pattern_length - (KMER_COUNTING_LENGTH-1) - KMER_COUNTING_LENGTH*max_error;
  memset(kmer_counting->kmer_count_text,0,KMER_COUNTING_NUM_KMERS*UINT16_SIZE);
  uint16_t* const kmer_count_text = kmer_counting->kmer_count_text;
  uint16_t* const kmer_count_pattern = kmer_counting->kmer_count_pattern;
  /*
   * First count (Load)
   */
  const uint64_t init_chunk = MIN(text_length,kmer_counting->pattern_length);
  uint64_t end_pos=0, kmer_idx_end=0, kmers_in_text = 0;
  const uint64_t total_kmers_text = MAX(text_length,kmer_counting->pattern_length);
  uint64_t kmers_left = total_kmers_text;
  // Initial fill
  for (;end_pos<KMER_COUNTING_LENGTH-1;++end_pos,--kmers_left) {
    const uint8_t enc_char_end = text[end_pos];
    KMER_COUNTING_ADD_INDEX__MASK(kmer_idx_end,enc_char_end); // Update kmer-index
  }
  for (;end_pos<init_chunk;++end_pos,--kmers_left) {
    const uint8_t enc_char_end = text[end_pos];
    KMER_COUNTING_ADD_INDEX__MASK(kmer_idx_end,enc_char_end); // Update kmer-index
    // Increment kmer-count
    const uint16_t count_pattern = kmer_count_pattern[kmer_idx_end];
    if (count_pattern>0 && (kmer_count_text[kmer_idx_end])++ < count_pattern) ++kmers_in_text;
    // Check filter condition
    if (kmers_in_text >= kmers_required) {
      PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
      return 0; // Don't filter
    } else if (kmers_required-kmers_in_text > kmers_left) { // Quick abandon
      PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
      return ALIGN_DISTANCE_INF; // Filter out
    }
  }
  // Check filter condition
  if (kmers_in_text >= kmers_required) {
    PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
    return 0; // Don't filter
  }
  if (init_chunk == text_length) {
    PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
    return ALIGN_DISTANCE_INF; // Filter out
  }
  /*
   * Sliding window count
   */
  // Initial fill
  uint64_t begin_pos = 0, kmer_idx_begin = 0;
  for (;begin_pos<KMER_COUNTING_LENGTH-1;++begin_pos) {
    const uint8_t enc_char_begin = text[begin_pos];
    KMER_COUNTING_ADD_INDEX(kmer_idx_begin,enc_char_begin); // Update kmer-index
  }
  for (;end_pos<text_length;++end_pos,++begin_pos,--kmers_left) {
    // Begin (Decrement kmer-count)
    const uint8_t enc_char_begin = text[begin_pos];
    KMER_COUNTING_ADD_INDEX__MASK(kmer_idx_begin,enc_char_begin);
    const uint16_t count_pattern_begin = kmer_count_pattern[kmer_idx_begin];
    if (count_pattern_begin > 0 && kmer_count_text[kmer_idx_begin]-- <= count_pattern_begin) --kmers_in_text;
    // End (Increment kmer-count)
    const uint8_t enc_char_end = text[end_pos];
    KMER_COUNTING_ADD_INDEX__MASK(kmer_idx_end,enc_char_end);
    const uint16_t count_pattern_end = kmer_count_pattern[kmer_idx_end];
    if (count_pattern_end > 0 && kmer_count_text[kmer_idx_end]++ < count_pattern_end) ++kmers_in_text;
    // Check filter condition
    if (kmers_in_text >= kmers_required) {
//      uint64_t n, total=0, kmers_ot = 0;
//      for (n=0;n<KMER_COUNTING_NUM_KMERS;++n) {
//        kmers_ot += kmer_count_text[n];
//        if (kmer_count_pattern[n]==0) {
//          fprintf(stderr," ");
//        } else if (kmer_count_text[n]>=kmer_count_pattern[n]) {
//          fprintf(stderr,"-"); total+=kmer_count_pattern[n];
//        } else {
//          fprintf(stderr,"*"); total+=kmer_count_text[n];
//        }
//      }
//      fprintf(stderr,"\n");
//      for (n=0;n<KMER_COUNTING_NUM_KMERS;++n) {
//        fprintf(stderr,"(%"PRIu64"/%"PRIu64")[%"PRIu64"] Pattern=%u Text=%u \n",total,kmers_ot,n,kmer_count_pattern[n],kmer_count_text[n]);
//      }
//      return begin_pos - (KMER_COUNTING_LENGTH-1); // Don't filter
      PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
      return 0; // Don't filter
    } else if (kmers_required-kmers_in_text > kmers_left) { // Quick abandon
      PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
      return ALIGN_DISTANCE_INF; // Filter out
    }
  }
  // Not passing the filter
  PROF_STOP(GP_FC_KMER_COUNTER_FILTER);
  return ALIGN_DISTANCE_INF; // Filter out
}
