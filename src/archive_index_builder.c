/*
 * PROJECT: GEMMapper
 * FILE: archive_builder.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 */

#include "archive_builder.h"
#include "input_fasta_parser.h"

/*
 * Suffix Sort
 */
// Sort Suffixes Debug prototypes
GEM_INLINE void archive_builder_sort_suffixes_debug_print_explicit_sa(archive_builder_t* const archive_builder);
GEM_INLINE void archive_builder_sort_suffixes_debug_print_bwt(archive_builder_t* const archive_builder,const bool verbose);
/*
 * STEP2 Archive Build :: Build BWT (SA)
 */
GEM_INLINE void archive_builder_build_bwt(
    archive_builder_t* const archive_builder,const bool dump_explicit_sa,const bool dump_bwt,const bool verbose) {
  // Allocate BWT-text
  const uint64_t text_length = dna_text_get_length(archive_builder->enc_text);
  archive_builder->enc_bwt = dna_text_new(text_length);
  dna_text_set_length(archive_builder->enc_bwt,text_length);
  // SA-Builder (SA-sorting)
  archive_builder->sa_builder = sa_builder_new(
      archive_builder->output_file_name_prefix,archive_builder->enc_text,
      archive_builder->num_threads,archive_builder->max_memory);
  // Count k-mers
  sa_builder_count_suffixes(archive_builder->sa_builder,archive_builder->character_occurrences,verbose);
  // Write SA-positions
  sa_builder_store_suffixes(archive_builder->sa_builder,verbose);
  // Sort suffixes & sample SA
  archive_builder->sampled_sa = sampled_sa_builder_new(text_length,archive_builder->sampling_rate); // Allocate Sampled-SA
  sa_builder_sort_suffixes(archive_builder->sa_builder,archive_builder->enc_bwt,archive_builder->sampled_sa,verbose);
  // DEBUG
  if (dump_bwt) archive_builder_sort_suffixes_debug_print_bwt(archive_builder,true);
  if (dump_explicit_sa) archive_builder_sort_suffixes_debug_print_explicit_sa(archive_builder);
  // Free
  sa_builder_delete(archive_builder->sa_builder); // Delete SA-Builder
}
/*
 * Debug functions
 */
GEM_INLINE void archive_builder_sort_suffixes_debug_print_explicit_sa(archive_builder_t* const archive_builder) {
  // Open File
  char* const debug_explicit_sa_file_name = gem_strcat(archive_builder->output_file_name_prefix,".sa");
  FILE* const debug_explicit_sa_file = fopen(debug_explicit_sa_file_name,"w");
  // Traverse SA & Print Explicit SA => (SApos,SA[SApos...SApos+SAFixLength])
  sa_builder_t* const sa_builder = archive_builder->sa_builder;
  const uint64_t sa_length = dna_text_get_length(sa_builder->enc_text);
  fm_t* const sa_positions_file = fm_open_file(sa_builder->sa_positions_file_name,FM_READ);
  uint64_t i;
  for (i=0;i<sa_length;++i) {
    sa_builder_debug_print_sa(
        debug_explicit_sa_file,sa_builder,
        fm_read_uint64(sa_positions_file),100);
  }
  // Free
  fm_close(sa_positions_file);
  fclose(debug_explicit_sa_file);
  mm_free(debug_explicit_sa_file_name);
}
GEM_INLINE void archive_builder_sort_suffixes_debug_print_bwt(archive_builder_t* const archive_builder,const bool verbose) {
  ticker_t ticker;
  ticker_percentage_reset(&ticker,verbose,"Building-BWT::Dumping BWT",1,1,true);
  char* const file_name = gem_strcat(archive_builder->output_file_name_prefix,".bwt");
  FILE* const bwt_file = fopen(file_name,"w"); // Open file
  dna_text_print_content(bwt_file,archive_builder->enc_bwt); // Dump BWT
  fclose(bwt_file);
  mm_free(file_name);
  ticker_finish(&ticker);
}