/*
 * PROJECT: GEMMapper
 * FILE: match_align.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 */

#include "match_align.h"
#include "match_align_dto.h"

/*
 * Curate Alignment
 */
GEM_INLINE bool match_align_curate_cigar_trim(
    match_align_parameters_t* const align_parameters,const cigar_element_t* const cigar_element,
    uint64_t* const trim_length,uint64_t* const match_position) {
  switch (cigar_element->type) {
    case cigar_match:
      // Small Match
      if (cigar_element->length < align_parameters->cigar_curation_min_end_context) {
        *trim_length += cigar_element->length;
        if (match_position!=NULL) *match_position += cigar_element->length;
        return true;
      }
    break;
    case cigar_mismatch:
      ++(*trim_length);
      if (match_position!=NULL) ++(*match_position);
      return true;
      break;
    case cigar_del:
      *trim_length += cigar_element->length;
      return true;
      break;
    case cigar_ins:
      if (match_position!=NULL) *match_position += cigar_element->length;
      return true;
      break;
    default:
      return false;
      break;
  }
  return false;
}
GEM_INLINE void match_align_curate_cigar(
    match_trace_t* const match_trace,vector_t* const cigar_vector,
    match_align_parameters_t* const align_parameters) {
  // Parameters
  const bool left_gap_alignment = align_parameters->left_gap_alignment;
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  const uint64_t cigar_length = match_alignment->cigar_length;
  cigar_element_t* const cigar_buffer = vector_get_elm(cigar_vector,match_alignment->cigar_offset,cigar_element_t);
  uint64_t curated_cigar_length = 0, indel_length = 0, i = 0, j = 0;
  // Trim the beginning of the read
  while (j < cigar_length) {
    cigar_element_t* const cigar_element = cigar_buffer + j;
    if (!match_align_curate_cigar_trim(align_parameters,cigar_element,&indel_length,&match_alignment->match_position)) {
      break;
    }
    // Trim
    cigar_element->type = cigar_del;
    cigar_element->attributes = cigar_attr_trim;
    cigar_element->length = indel_length;
    i = j++;
  }
  // Traverse all CIGAR elements
  while (i < cigar_length) {
    cigar_element_t* cigar_element = cigar_buffer + i;
    switch (cigar_element->type) {
      case cigar_mismatch:
        cigar_buffer[curated_cigar_length++] = *cigar_element;
        ++i;
        break;
      case cigar_match:
        if (i+1<cigar_length && cigar_buffer[i+1].type == cigar_match) {
          cigar_buffer[i+1].length += cigar_element->length;
        } else {
          cigar_buffer[curated_cigar_length++] = *cigar_element;
        }
        ++i;
        break;
      case cigar_del:
      case cigar_ins: {
        cigar_element_t accum_del = { .type = cigar_del, .attributes = cigar_attr_none, .length = 0 };
        cigar_element_t accum_ins = { .type = cigar_ins, .attributes = cigar_attr_none, .length = 0 };
        if (cigar_element->type==cigar_del && cigar_element->attributes==cigar_attr_trim) {
          accum_del.attributes = cigar_attr_trim;
        }
        // Compact all deletions/insertions in a row
        while (i<cigar_length) {
          cigar_element = cigar_buffer + i;
          if (cigar_element->type == cigar_del) {
            accum_del.length += cigar_element->length;
            ++i;
          } else if (cigar_element->type == cigar_ins) {
            accum_ins.length += cigar_element->length;
            ++i;
          } else {
            break;
          }
        }
        // Copy curated Ins/Del
        if (left_gap_alignment) {
          if (accum_del.length > 0) cigar_buffer[curated_cigar_length++] = accum_del;
          if (accum_ins.length > 0) cigar_buffer[curated_cigar_length++] = accum_ins;
        } else {
          if (accum_ins.length > 0) cigar_buffer[curated_cigar_length++] = accum_ins;
          if (accum_del.length > 0) cigar_buffer[curated_cigar_length++] = accum_del;
        }
        break;
      }
      default:
        GEM_INVALID_CASE();
        break;
    }
  }
  // Handle last CIGAR element(s)
  if (curated_cigar_length > 0) {
    // Check match or deletion at the end
    cigar_element_t* last_cigar_element = cigar_buffer + (curated_cigar_length-1);
    uint64_t indel_length = 0;
    if (match_align_curate_cigar_trim(align_parameters,last_cigar_element,&indel_length,NULL)) {
      // Chain all the mismatches & deletions at the end
      while (last_cigar_element > cigar_buffer) {
        --last_cigar_element;
        if (!match_align_curate_cigar_trim(align_parameters,last_cigar_element,&indel_length,NULL)) break;
        --curated_cigar_length;
      }
      // Merge all of them
      last_cigar_element = cigar_buffer + (curated_cigar_length-1);
      last_cigar_element->type = cigar_del;
      last_cigar_element->attributes = cigar_attr_trim;
      last_cigar_element->length = indel_length;
      if (indel_length==0) --curated_cigar_length;
    }
  }
  // Set curated-CIGAR length
  match_alignment->cigar_length = curated_cigar_length;
}
/*
 * Exact Alignment
 *   @align_input->key_length
 *   @align_input->text_position
 *   @align_input->text_trace_offset
 *   @align_input->text
 *   @align_input->text_offset_begin
 *   @align_input->text_offset_end
 *   @align_parameters->emulated_rc_search
 *   @align_parameters->swg_penalties
 *   @match_trace->match_alignment.score
 */
GEM_INLINE void match_align_exact(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters) {
  PROF_START(GP_MATCHES_ALIGN_EXACT);
  // Parameters
  const uint64_t key_length = align_input->key_length;
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  // Configure match-trace
  match_trace->trace_offset = align_input->text_trace_offset;
  match_trace->text = (match_alignment->score==0) ?
      align_input->text + align_input->text_offset_begin : align_input->text;
  match_trace->text_length = align_input->key_length;
  match_trace->sequence_name = NULL;
  match_trace->text_position = UINT64_MAX;
  match_trace->emulated_rc_search = align_parameters->emulated_rc_search;
  match_trace->distance = match_alignment->score;
  match_trace->edit_distance = match_alignment->score;
  match_trace->swg_score = swg_score_match(align_parameters->swg_penalties,(int32_t)key_length);
  // Insert exact-match CIGAR
  match_alignment->match_position = (match_alignment->score==0) ? // Adjust position (if needed)
      align_input->text_position + align_input->text_offset_begin : align_input->text_position;
  match_alignment->cigar_offset = vector_get_used(matches->cigar_vector);
  match_alignment->cigar_length = 0;
  match_alignment->effective_length = key_length;
  matches_cigar_vector_append_match(matches->cigar_vector,&match_alignment->cigar_length,key_length,cigar_attr_none);
  PROF_STOP(GP_MATCHES_ALIGN_EXACT);
}
/*
 * Hamming Alignment (Only mismatches)
 *   @align_input->key
 *   @align_input->key_length
 *   @align_input->text_position
 *   @align_input->text_trace_offset
 *   @align_input->text
 *   @align_input->text_offset_begin
 *   @align_input->text_offset_end
 *   @align_parameters->emulated_rc_search
 *   @align_parameters->allowed_enc
 */
GEM_INLINE void match_align_hamming(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters) {
  PROF_START(GP_MATCHES_ALIGN_HAMMING);
  // Parameters
  const uint8_t* const key = align_input->key;
  const uint64_t key_length = align_input->key_length;
  uint8_t* const text = align_input->text;
  const uint64_t text_offset_begin = align_input->text_offset_begin;
  const uint64_t text_offset_end = align_input->text_offset_end;
  const bool* const allowed_enc = align_parameters->allowed_enc;
  // Configure match-trace
  match_trace->trace_offset = align_input->text_trace_offset;
  match_trace->text = text + text_offset_begin;
  match_trace->text_length = key_length;
  match_trace->sequence_name = NULL;
  match_trace->text_position = UINT64_MAX;
  match_trace->emulated_rc_search = align_parameters->emulated_rc_search;
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  match_alignment->match_position = align_input->text_position;
  match_alignment->cigar_offset = vector_get_used(matches->cigar_vector);
  match_alignment->cigar_length = 0;
  // Hamming Alignment
  uint64_t i, mismatches;
  for (i=text_offset_begin,mismatches=0;i<text_offset_end;++i) {
    const uint8_t candidate_enc = text[i];
    // Check Mismatch
    if (!allowed_enc[candidate_enc] || candidate_enc != key[i]) {
      ++mismatches;
      matches_cigar_vector_append_mismatch(matches->cigar_vector,&match_alignment->cigar_length,candidate_enc,cigar_attr_none);
    } else {
      matches_cigar_vector_append_match(matches->cigar_vector,&match_alignment->cigar_length,1,cigar_attr_none);
    }
  }
  match_alignment->effective_length = key_length;
  match_trace->distance = mismatches;
  match_trace->edit_distance = mismatches;
  match_trace->swg_score = swg_score_cigar(align_parameters->swg_penalties,
      matches->cigar_vector,match_alignment->cigar_offset,match_alignment->cigar_length);
  PROF_STOP(GP_MATCHES_ALIGN_HAMMING);
}
/*
 * Levenshtein
 *   @align_input->key
 *   @align_input->key_length
 *   @align_input->bpm_pattern
 *   @align_input->text_trace_offset
 *   @align_input->text_position
 *   @align_input->text
 *   @align_input->text_offset_begin
 *   @align_input->text_offset_end
 *   @align_input->text_length
 *   @align_parameters->swg_penalties
 *   @align_parameters->emulated_rc_search
 *   @align_parameters->max_error
 *   @align_parameters->left_gap_alignment
 */
GEM_INLINE void match_align_levenshtein(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters,
    mm_stack_t* const mm_stack) {
  PROF_START(GP_MATCHES_ALIGN_LEVENSHTEIN);
  // Configure match-trace
  match_trace->trace_offset = align_input->text_trace_offset;
  match_trace->sequence_name = NULL;
  match_trace->text_position = UINT64_MAX;
  match_trace->emulated_rc_search = align_parameters->emulated_rc_search;
  // Levenshtein Align
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  align_input->text += align_input->text_offset_begin;
  align_input->text_length = align_input->text_offset_end - align_input->text_offset_begin;
  const uint64_t match_position = align_input->text_position + align_input->text_offset_begin;
  match_alignment->match_position = match_position;
  bpm_align_match(align_input,align_parameters->max_error,
      align_parameters->left_gap_alignment,match_alignment,matches->cigar_vector,mm_stack);
  match_trace->distance = match_alignment->score;
  match_trace->edit_distance = match_alignment->score;
  match_trace->swg_score = swg_score_cigar(align_parameters->swg_penalties,
      matches->cigar_vector,match_alignment->cigar_offset,match_alignment->cigar_length);
  // Store matching text
  match_trace->text = align_input->text + (match_alignment->match_position - match_position);
  match_trace->text_length = match_alignment->effective_length;
  PROF_STOP(GP_MATCHES_ALIGN_LEVENSHTEIN);
}
/*
 * SWG Align Matching Region (@region_matching)
 *   @align_input->key
 *   @align_input->text
 *   @align_parameters->swg_penalties
 *   @align_parameters->left_gap_alignment
 *   @align_parameters->allowed_enc
 *   @align_parameters->max_bandwidth
 *   @match_alignment->cigar_length (Cumulative)
 */
GEM_INLINE void match_align_swg_add_region_matching(
    const region_matching_t* const region_matching,match_align_input_t* const align_input,
    match_align_parameters_t* const align_parameters,match_alignment_t* const match_alignment,
    vector_t* const cigar_vector,mm_stack_t* const mm_stack) {
  // Parameters
  uint8_t* const key = align_input->key;
  const uint64_t key_matching_length = region_matching->key_end - region_matching->key_begin;
  uint8_t* const text = align_input->text;
  const uint64_t text_matching_length = region_matching->text_end - region_matching->text_begin;
  // Select matching-region type
  switch (region_matching->matching_type) {
    case region_matching_exact:
      matches_cigar_vector_append_match(cigar_vector,&match_alignment->cigar_length,key_matching_length,cigar_attr_none);
      match_alignment->score = swg_score_match(align_parameters->swg_penalties,key_matching_length);
      break;
    case region_matching_approximate:
      if (region_matching->cigar_length > 0) {
        // Copy the CIGAR from the matching region
        const uint64_t cigar_buffer_offset = region_matching->cigar_buffer_offset;
        uint64_t i;
        match_alignment->score = 0;
        for (i=0;i<region_matching->cigar_length;++i) {
          cigar_element_t* const scaffolding_elm = vector_get_elm(cigar_vector,cigar_buffer_offset+i,cigar_element_t);
          match_alignment->score += swg_score_cigar_element(align_parameters->swg_penalties,scaffolding_elm);
          matches_cigar_vector_append_cigar_element(cigar_vector,&match_alignment->cigar_length,scaffolding_elm);
        }
      } else {
        // Force (re)computing the CIGAR from the matching region (missing CIGAR or never computed)
        match_align_input_t align_chunk_input = {
            .key = key+region_matching->key_begin,
            .key_length = key_matching_length,
            .text = text+region_matching->text_begin,
            .text_length = text_matching_length,
        };
        match_align_parameters_t align_chunk_parameters = {
            .max_bandwidth = region_matching->error+1, // FIXME Include CIGAR when generating approximate regions
            .left_gap_alignment = align_parameters->left_gap_alignment,
            .allowed_enc = align_parameters->allowed_enc,
            .swg_penalties = align_parameters->swg_penalties,
        };
        swg_align_match(&align_chunk_input,&align_chunk_parameters,false,false,match_alignment,cigar_vector,mm_stack);
      }
      break;
    default:
      GEM_INVALID_CASE();
      break;
  }
}
/*
 * SWG Bride the gap
 */
GEM_INLINE void match_align_swg_bride_gap(
    matches_t* const matches,match_alignment_t* const match_alignment,
    const uint64_t key_chunk_length,const uint64_t text_chunk_length,const bool trim) {
  vector_t* const cigar_vector = matches->cigar_vector;
  if (trim) {
    // Trim the alignment
    if (key_chunk_length > 0) {
      matches_cigar_vector_append_deletion(cigar_vector,
          &match_alignment->cigar_length,key_chunk_length,cigar_attr_trim);
    }
  } else {
    // Delete the read chunk
    if (key_chunk_length > 0) {
      matches_cigar_vector_append_deletion(cigar_vector,
          &match_alignment->cigar_length,key_chunk_length,cigar_attr_none);
    }
    // Insert the text chunk
    if (text_chunk_length > 0) {
      matches_cigar_vector_append_insertion(cigar_vector,
          &match_alignment->cigar_length,text_chunk_length,cigar_attr_none);
    }
  }
}
/*
 * SWG Align region
 */
GEM_INLINE bool match_align_swg_region(
    matches_t* const matches,match_align_input_t* const align_input,
    match_align_parameters_t* const align_parameters,match_alignment_t* const match_alignment,
    const uint64_t key_chunk_begin_offset,const uint64_t key_chunk_length,
    const uint64_t text_chunk_begin_offset,const uint64_t text_chunk_length,
    const bool begin_free,const bool end_free,
    const bool force_swg_threshold,mm_stack_t* const mm_stack) {
  vector_t* const cigar_vector = matches->cigar_vector;
  // Align gap between regions matching
  match_align_input_t align_chunk_input;
  align_chunk_input.key = align_input->key+key_chunk_begin_offset;
  align_chunk_input.key_length = key_chunk_length;
  align_chunk_input.text = align_input->text+text_chunk_begin_offset;
  align_chunk_input.text_length = text_chunk_length;
  // Keep CIGAR state
  const uint64_t cigar_length = match_alignment->cigar_length;
  const uint64_t cigar_vector_used = vector_get_used(cigar_vector);
  // SWG-Align
  swg_align_match(&align_chunk_input,align_parameters,begin_free,end_free,match_alignment,cigar_vector,mm_stack);
  // Check alignment result
  if (match_alignment->score == SWG_SCORE_MIN || (force_swg_threshold && match_alignment->score < align_parameters->swg_threshold)) {
    // Restore the CIGAR
    match_alignment->cigar_length = cigar_length;
    vector_set_used(cigar_vector,cigar_vector_used);
    // Bride the gap
    match_align_swg_bride_gap(matches,match_alignment,key_chunk_length,text_chunk_length,begin_free||end_free);
    return false;
  } else {
    return true;
  }
}
GEM_INLINE bool match_align_swg_middle_region(
    matches_t* const matches,match_align_input_t* const align_input,
    match_align_parameters_t* const align_parameters,match_alignment_t* const match_alignment,
    const uint64_t key_chunk_begin_offset,const uint64_t key_chunk_length,
    const uint64_t text_chunk_begin_offset,const uint64_t text_chunk_length,
    const bool force_swg_threshold,mm_stack_t* const mm_stack) {
  return match_align_swg_region(matches,align_input,align_parameters,match_alignment,
      key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,
      false,false,force_swg_threshold,mm_stack);
}
GEM_INLINE bool match_align_swg_begin_region(
    matches_t* const matches,match_align_input_t* const align_input,
    match_align_parameters_t* const align_parameters,match_alignment_t* const match_alignment,
    const uint64_t key_chunk_begin_offset,const uint64_t key_chunk_length,
    const uint64_t text_chunk_begin_offset,const uint64_t text_chunk_length,
    const bool force_swg_threshold,mm_stack_t* const mm_stack) {
  return match_align_swg_region(matches,align_input,align_parameters,match_alignment,
      key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,
      true,false,force_swg_threshold,mm_stack);
}
GEM_INLINE bool match_align_swg_end_region(
    matches_t* const matches,match_align_input_t* const align_input,
    match_align_parameters_t* const align_parameters,match_alignment_t* const match_alignment,
    const uint64_t key_chunk_begin_offset,const uint64_t key_chunk_length,
    const uint64_t text_chunk_begin_offset,const uint64_t text_chunk_length,
    const bool force_swg_threshold,mm_stack_t* const mm_stack) {
  return match_align_swg_region(matches,align_input,align_parameters,match_alignment,
      key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,
      false,true,force_swg_threshold,mm_stack);
}
/*
 * Chained SWG Alignment
 *   @align_input->key
 *   @align_input->key_length
 *   @align_input->text
 *   @align_input->text_length
 *   @align_parameters->swg_penalties
 *   @align_parameters->left_gap_alignment
 *   @align_parameters->allowed_enc
 *   @align_parameters->max_bandwidth
 *   @match_scaffold->scaffold_regions
 *   @match_scaffold->num_scaffold_regions
 *   @match_alignment->cigar_length (Cumulative)
 *   @match_alignment->effective_length (Cumulative)
 *   @match_alignment->score (Cumulative)
 */
GEM_INLINE void match_align_swg_chain_scaffold(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters,
    match_scaffold_t* const match_scaffold,mm_stack_t* const mm_stack) {
  // Parameters
  const uint64_t key_length = align_input->key_length;
  const uint64_t text_length = align_input->text_length;
  const uint64_t max_bandwidth = align_parameters->max_bandwidth;
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  region_matching_t* const scaffold_regions = match_scaffold->scaffold_regions;
  const uint64_t num_scaffold_regions = match_scaffold->num_scaffold_regions;
  vector_t* const cigar_vector = matches->cigar_vector;
  // Auxiliary Variables
  uint64_t key_chunk_begin_offset, key_chunk_length, i;
  uint64_t text_chunk_end_offset, text_chunk_begin_offset, text_chunk_length, match_begin_position;
  // Chain to first matching region
  const region_matching_t* const first_region_matching = scaffold_regions;
  key_chunk_length = first_region_matching->key_begin;
  key_chunk_begin_offset = 0;
  text_chunk_length = BOUNDED_ADDITION(key_chunk_length,max_bandwidth,first_region_matching->text_begin);
  text_chunk_begin_offset = first_region_matching->text_begin - text_chunk_length;
  match_alignment->match_position += text_chunk_begin_offset; // Offset match position
  match_begin_position = match_alignment->match_position; // Save match position
  const bool feasible_alignment = match_align_swg_begin_region(matches,
      align_input,align_parameters,match_alignment,key_chunk_begin_offset,
      key_chunk_length,text_chunk_begin_offset,text_chunk_length,false,mm_stack);
  match_begin_position = (feasible_alignment) ? match_alignment->match_position : match_begin_position + text_chunk_length;
  // Add first matching region
  match_align_swg_add_region_matching(first_region_matching,
      align_input,align_parameters,match_alignment,cigar_vector,mm_stack);
  // Chain matching regions
  for (i=1;i<num_scaffold_regions;++i) {
    // Align the gap
    const region_matching_t* const prev_region_matching = scaffold_regions + (i-1);
    const region_matching_t* const current_region_matching = scaffold_regions + i;
    const uint64_t key_chunk_begin_offset = prev_region_matching->key_end;
    const uint64_t key_chunk_length = current_region_matching->key_begin - key_chunk_begin_offset;
    const uint64_t text_chunk_begin_offset = prev_region_matching->text_end;
    const uint64_t text_chunk_length = current_region_matching->text_begin - text_chunk_begin_offset;
    match_align_swg_middle_region(matches,align_input,align_parameters,match_alignment,
        key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,false,mm_stack);
    // Add matching region
    match_align_swg_add_region_matching(current_region_matching,
        align_input,align_parameters,match_alignment,cigar_vector,mm_stack);
  }
  // Chain from last matching region
  const region_matching_t* const last_region_matching = scaffold_regions + (num_scaffold_regions-1);
  key_chunk_begin_offset = last_region_matching->key_end;
  key_chunk_length = key_length - last_region_matching->key_end;
  text_chunk_end_offset = BOUNDED_ADDITION(last_region_matching->text_end,key_chunk_length+max_bandwidth,text_length);
  text_chunk_begin_offset = last_region_matching->text_end;
  text_chunk_length = text_chunk_end_offset-last_region_matching->text_end;
  match_align_swg_end_region(matches,align_input,align_parameters,match_alignment,
      key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,false,mm_stack);
  // Post-processing
  match_alignment->match_position = match_begin_position; // Restore match position
}
/*
 * Smith-Waterman-Gotoh Post-Alignment (Curate cigar, compute metrics, filter bad-alignments)
 */
GEM_INLINE void match_align_swg_post_alignment(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters,
    const bool local_alignment) {
  vector_t* const cigar_vector = matches->cigar_vector;
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  // Check for bad alignments (discarded)
  if (match_alignment->score == SWG_SCORE_MIN) {
    match_trace->swg_score = SWG_SCORE_MIN;
    match_trace->distance = ALIGN_DISTANCE_INF;
    PROF_STOP(GP_MATCHES_ALIGN_SWG);
    return;
  }
  // Curate alignment
  if (align_parameters->cigar_curation) {
    match_align_curate_cigar(match_trace,cigar_vector,align_parameters);
  }
  // Discard bad alignments & Compute edit distance
  const uint64_t cigar_offset = match_alignment->cigar_offset;
  const uint64_t cigar_length = match_alignment->cigar_length;
  const uint64_t matching_bases = matches_cigar_compute_matching_bases(matches,cigar_offset,cigar_length);
  if (matching_bases < align_parameters->min_identity) {
    match_trace->swg_score = SWG_SCORE_MIN;
    match_trace->distance = ALIGN_DISTANCE_INF;
    PROF_STOP(GP_MATCHES_ALIGN_SWG);
    return;
  }
  match_trace->edit_distance = matches_cigar_compute_edit_distance(matches,cigar_offset,cigar_length);
  // Compute score & discard alignments below threshold
//  match_trace->swg_score = (!local_alignment) ?
//      swg_score_cigar(align_parameters->swg_penalties,cigar_vector,cigar_offset,cigar_length) :
//      swg_score_cigar__excluding_clipping(align_parameters->swg_penalties,cigar_vector,cigar_offset,cigar_length);
  match_trace->swg_score = swg_score_cigar(align_parameters->swg_penalties,cigar_vector,cigar_offset,cigar_length);
  if (!local_alignment && match_trace->swg_score < align_parameters->swg_threshold) {
    match_trace->swg_score = SWG_SCORE_MIN;
    match_trace->distance = ALIGN_DISTANCE_INF;
    PROF_STOP(GP_MATCHES_ALIGN_SWG);
    return;
  }
  // Compute distance + effective-length
  match_trace->distance = matches_cigar_compute_event_distance(matches,cigar_offset,cigar_length);
  match_alignment->effective_length = matches_cigar_effective_length(cigar_vector,cigar_offset,cigar_length);
}
/*
 * Smith-Waterman-Gotoh Alignment (Gap-affine)
 *   @align_input->key
 *   @align_input->key_length
 *   @align_input->text_trace_offset
 *   @align_input->text_position
 *   @align_input->text
 *   @align_input->text_length
 *   @align_input->text_offset_begin
 *   @align_input->text_offset_end
 *   @align_parameters->emulated_rc_search
 *   @align_parameters->swg_penalties
 *   @align_parameters->swg_threshold
 *   @align_parameters->min_identity
 *   @align_parameters->left_gap_alignment
 *   @align_parameters->allowed_enc
 *   @align_parameters->max_bandwidth
 *   @align_parameters->cigar_curation
 *   @align_parameters->cigar_curation_min_end_context
 *   @match_scaffold->scaffold_regions
 *   @match_scaffold->num_scaffold_regions
 */
GEM_INLINE void match_align_smith_waterman_gotoh(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters,
    match_scaffold_t* const match_scaffold,mm_stack_t* const mm_stack) {
  PROF_START(GP_MATCHES_ALIGN_SWG);
  // Configure match-trace
  match_trace->trace_offset = align_input->text_trace_offset;
  match_trace->sequence_name = NULL;
  match_trace->text_position = UINT64_MAX;
  match_trace->emulated_rc_search = align_parameters->emulated_rc_search;
  // Scaffold the alignment
  if (align_parameters->scaffolding) {
    match_scaffold_alignment(matches,align_input,align_parameters,match_scaffold,mm_stack);
//    // Check minimum identity
//    if (align_parameters->min_identity + match_scaffold->match_alignment.score > align_input->key_length) {
//      match_scaffold->num_scaffold_regions = 0;
//      match_scaffold->scaffolding_coverage = 0;
//    }
  }
  const uint64_t num_scaffold_regions = match_scaffold->num_scaffold_regions;
#ifdef GEM_DEBUG
  match_trace->match_scaffold = (match_scaffold!=NULL && num_scaffold_regions > 0) ? match_scaffold : NULL;
#endif
  // Configure match-alignment
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  const uint64_t base_match_position = align_input->text_position;
  match_alignment->match_position = base_match_position;
  match_alignment->cigar_offset = vector_get_used(matches->cigar_vector);
  match_alignment->cigar_length = 0;
  // Check the number of matching regions
  if (match_scaffold!=NULL && num_scaffold_regions > 0) {
    // Chain matching regions and align gaps (SWG)
    match_align_swg_chain_scaffold(matches,match_trace,align_input,align_parameters,match_scaffold,mm_stack);
  } else {
    // Full SWG
    match_align_input_t align_chunk_input;
    align_chunk_input.key = align_input->key;
    align_chunk_input.key_length = align_input->key_length;
    align_chunk_input.text = align_input->text + align_input->text_offset_begin;
    align_chunk_input.text_length = align_input->text_offset_end - align_input->text_offset_begin;
    match_alignment->match_position += align_input->text_offset_begin;
    swg_align_match(&align_chunk_input,align_parameters,true,true,
        &match_trace->match_alignment,matches->cigar_vector,mm_stack);
  }
  // Post alignment checks & setup
  match_align_swg_post_alignment(matches,match_trace,align_input,align_parameters,false);
  // Adjust text
  match_trace->text = align_input->text + (match_alignment->match_position - base_match_position);
  match_trace->text_length = match_alignment->effective_length;
  PROF_STOP(GP_MATCHES_ALIGN_SWG);
}
/*
 * Chained local-SWG Alignment
 */
GEM_INLINE void match_align_local_swg_chain_local_max(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters,
    match_scaffold_t* const match_scaffold,mm_stack_t* const mm_stack) {
  // Parameters
  const uint64_t key_length = align_input->key_length;
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  region_matching_t* const scaffold_regions = match_scaffold->scaffold_regions;
  const uint64_t num_scaffold_regions = match_scaffold->num_scaffold_regions;
  vector_t* const cigar_vector = matches->cigar_vector;
  // Auxiliary Variables
  const uint64_t match_text_base_position = match_alignment->match_position;
  uint64_t i, key_chunk_begin_offset, key_chunk_length;
  uint64_t text_chunk_begin_offset, text_chunk_length, match_begin_position;
  bool begin_region = true;
  // Keep CIGAR state
  const uint64_t cigar_length = match_alignment->cigar_length;
  const uint64_t cigar_vector_used = vector_get_used(cigar_vector);
  // Chain matching regions
  for (i=0;i<num_scaffold_regions;++i) {
    const region_matching_t* const current_region_matching = scaffold_regions + i;
    key_chunk_begin_offset = current_region_matching->key_begin;
    key_chunk_length = current_region_matching->key_end - current_region_matching->key_begin;
    text_chunk_begin_offset = current_region_matching->text_begin;
    text_chunk_length = current_region_matching->text_end - text_chunk_begin_offset;
    if (begin_region) {
      // Trim the alignment (to first matching region)
      match_alignment->match_position = match_text_base_position + current_region_matching->text_begin;
      match_align_swg_bride_gap(matches,match_alignment,current_region_matching->key_begin,0,true);
      // Align region
      begin_region = !match_align_swg_begin_region(matches,align_input,align_parameters,match_alignment,
          key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,true,mm_stack);
      if (begin_region) {
        // Restore the CIGAR
        match_alignment->cigar_length = cigar_length;
        vector_set_used(cigar_vector,cigar_vector_used);
      } else {
        match_begin_position = match_alignment->match_position;
      }
    } else {
      // Align region
      if (i+1 < num_scaffold_regions) {
        match_align_swg_middle_region(matches,align_input,align_parameters,match_alignment,
            key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,true,mm_stack);
      } else {
        match_align_swg_end_region(matches,align_input,align_parameters,match_alignment,
            key_chunk_begin_offset,key_chunk_length,text_chunk_begin_offset,text_chunk_length,true,mm_stack);
      }
    }
    // Bride the gap
    if (!begin_region) {
      if (i+1 < num_scaffold_regions) {
        // Bride the gap
        const region_matching_t* const next_region_matching = current_region_matching + 1;
        key_chunk_begin_offset = current_region_matching->key_end;
        key_chunk_length = next_region_matching->key_begin - key_chunk_begin_offset;
        text_chunk_begin_offset = current_region_matching->text_end;
        text_chunk_length = next_region_matching->text_begin - text_chunk_begin_offset;
        match_align_swg_bride_gap(matches,match_alignment,key_chunk_length,text_chunk_length,false);
      } else {
        // Trim the alignment (from last matching region)
        const region_matching_t* const last_region_matching = scaffold_regions + (num_scaffold_regions-1);
        key_chunk_length = key_length - last_region_matching->key_end;
        match_align_swg_bride_gap(matches,match_alignment,key_chunk_length,0,true);
      }
    }
  }
  // Post-processing
  if (begin_region) {
    match_alignment->score = SWG_SCORE_MIN;
  } else {
    match_alignment->match_position = match_begin_position; // Restore match position
  }
}
/*
 * Local Smith-Waterman-Gotoh Alignment (Gap-affine)
 *   @align_input->key
 *   @align_input->key_length
 *   @align_input->text_trace_offset
 *   @align_input->text_position
 *   @align_input->text
 *   @align_input->text_length
 *   @align_input->text_offset_begin
 *   @align_input->text_offset_end
 *   @align_parameters->emulated_rc_search
 *   @align_parameters->swg_penalties
 *   @align_parameters->swg_threshold
 *   @align_parameters->min_identity
 *   @align_parameters->left_gap_alignment
 *   @align_parameters->allowed_enc
 *   @align_parameters->max_bandwidth
 *   @align_parameters->scaffolding
 *   @align_parameters->scaffolding_min_coverage
 *   @align_parameters->scaffolding_matching_min_length
 *   @align_parameters->scaffolding_homopolymer_min_context
 *   @align_parameters->cigar_curation
 *   @align_parameters->cigar_curation_min_end_context
 *   @match_scaffold->scaffold_regions
 *   @match_scaffold->num_scaffold_regions
 */
GEM_INLINE void match_align_local_smith_waterman_gotoh(
    matches_t* const matches,match_trace_t* const match_trace,
    match_align_input_t* const align_input,match_align_parameters_t* const align_parameters,
    match_scaffold_t* const match_scaffold,mm_stack_t* const mm_stack) {
  PROF_START(GP_MATCHES_ALIGN_LOCAL_SWG);
  // Configure match-trace
  match_trace->trace_offset = align_input->text_trace_offset;
  match_trace->sequence_name = NULL;
  match_trace->text_position = UINT64_MAX;
  match_trace->emulated_rc_search = align_parameters->emulated_rc_search;
  // Scaffold the alignment
  if (!match_scaffold_smith_waterman_gotoh(matches,align_input,align_parameters,match_scaffold,mm_stack)) {
    match_trace->distance=ALIGN_DISTANCE_INF;
    return;
  }
  const uint64_t num_scaffold_regions = match_scaffold->num_scaffold_regions;
#ifdef GEM_DEBUG
  match_trace->match_scaffold = (match_scaffold!=NULL && num_scaffold_regions > 0) ? match_scaffold : NULL;
#endif
  // Configure match-alignment
  match_alignment_t* const match_alignment = &match_trace->match_alignment;
  const uint64_t base_match_position = align_input->text_position;
  match_alignment->match_position = base_match_position;
  match_alignment->cigar_offset = vector_get_used(matches->cigar_vector);
  match_alignment->cigar_length = 0;
  // Chain SWG-matching regions
  match_align_local_swg_chain_local_max(matches,match_trace,align_input,align_parameters,match_scaffold,mm_stack);
  // Post alignment checks & setup
  match_align_swg_post_alignment(matches,match_trace,align_input,align_parameters,true);
//  // Check correctness
//  align_check_match(stderr,align_input->key,align_input->key_length,
//      align_input->text+(match_alignment->match_position - base_match_position),match_alignment->effective_length,
//      matches->cigar_vector,match_trace->match_alignment.cigar_offset,match_trace->match_alignment.cigar_length,true);
  // Adjust text
  match_trace->text = align_input->text + (match_alignment->match_position - base_match_position);
  match_trace->text_length = match_alignment->effective_length;
  PROF_STOP(GP_MATCHES_ALIGN_LOCAL_SWG);
}
