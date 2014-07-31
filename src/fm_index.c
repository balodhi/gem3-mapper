/*
 * PROJECT: GEMMapper
 * FILE: fm_index.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 */

#include "fm_index.h"

/*
 * Builder
 */
GEM_INLINE void fm_index_builder(
    fm_t* const file_manager,
    dna_text_t* const bwt_text,uint64_t* const character_occurrences,
    sampled_sa_builder_t* const sampled_sa,
    const bool check,const bool verbose,const uint64_t num_threads) {
  /*
   * Write Header
   */
  const uint64_t text_length = dna_text_get_length(bwt_text);
  const uint64_t proper_length = log2(text_length)/2;
  fm_write_uint64(file_manager,text_length);
  fm_write_uint64(file_manager,proper_length);
  /*
   * Write Sampled SA & Free
   */
  sampled_sa_builder_write(file_manager,sampled_sa);
  if (verbose) sampled_sa_builder_print(gem_info_get_stream(),sampled_sa);
  sampled_sa_builder_delete(sampled_sa); // Free
  /*
   * Generate BWT & rank_mtable
   */
  bwt_builder_t* const bwt_builder =
      bwt_builder_new(bwt_text,character_occurrences,check,verbose,mm_pool_get_slab(mm_pool_32MB));
  if (verbose) bwt_builder_print(gem_info_get_stream(),bwt_builder);
  // Free BWT-text
  dna_text_delete(bwt_text);
  // Build mrank table
  rank_mtable_t* const rank_mtable = rank_mtable_builder_new(bwt_builder,verbose);
  if (verbose) rank_mtable_print(gem_info_get_stream(),rank_mtable);
  // Write mrank table
  rank_mtable_write(file_manager,rank_mtable);
  rank_mtable_builder_delete(rank_mtable); // Free
  // Write BWT
  bwt_builder_write(file_manager,bwt_builder);
  bwt_builder_delete(bwt_builder); // Free
}
/*
 * Loader
 */
GEM_INLINE fm_index_t* fm_index_read(fm_t* const file_manager,const bool check) {
  // Allocate handler
  fm_index_t* const fm_index = mm_alloc(fm_index_t);
  // Read Header
  fm_index->text_length = fm_read_uint64(file_manager);
  fm_index->proper_length = fm_read_uint64(file_manager);
  // Load Sampled SA
  fm_index->sampled_sa = sampled_sa_read(file_manager);
  // Load rank_mtable
  fm_index->rank_table = rank_mtable_read(file_manager);
  // Load BWT
  fm_index->bwt = bwt_read(file_manager,check);
  // Return
  return fm_index;
}
GEM_INLINE fm_index_t* fm_index_read_mem(mm_t* const memory_manager,const bool check) {
  // Allocate handler
  fm_index_t* const fm_index = mm_alloc(fm_index_t);
  // Read Header
  fm_index->text_length = mm_read_uint64(memory_manager);
  fm_index->proper_length = mm_read_uint64(memory_manager);
  // Load Sampled SA
  fm_index->sampled_sa = sampled_sa_read_mem(memory_manager);
  // Load rank_mtable
  fm_index->rank_table = rank_mtable_read_mem(memory_manager);
  // Load BWT
  fm_index->bwt = bwt_read_mem(memory_manager,check);
  // Return
  return fm_index;
}
GEM_INLINE bool fm_index_check(const fm_index_t* const fm_index,const bool verbose) {
  FM_INDEX_CHECK(fm_index);
  // TODO
  return true;
}
GEM_INLINE void fm_index_delete(fm_index_t* const fm_index) {
  FM_INDEX_CHECK(fm_index);
  // Delete Sampled SA
  sampled_sa_delete(fm_index->sampled_sa);
  // Delete rank_mtable
  rank_mtable_delete(fm_index->rank_table);
  // Delete BWT
  bwt_delete(fm_index->bwt);
  // Free handler
  mm_free(fm_index);
}

/*
 * Accessors
 */
GEM_INLINE uint64_t fm_index_get_length(const fm_index_t* const fm_index) {
  FM_INDEX_CHECK(fm_index);
  return fm_index->text_length;
}
GEM_INLINE double fm_index_get_proper_length(const fm_index_t* const fm_index) {
  FM_INDEX_CHECK(fm_index);
  return fm_index->proper_length;
}
GEM_INLINE uint64_t fm_index_get_size(const fm_index_t* const fm_index) {
  const uint64_t sampled_sa_size = sampled_sa_get_size(fm_index->sampled_sa); // Sampled SuffixArray positions
  const uint64_t bwt_size = bwt_get_size(fm_index->bwt); // BWT structure
  const uint64_t rank_table_size = rank_mtable_get_size(fm_index->rank_table); // Memoizated intervals
  return sampled_sa_size + bwt_size + rank_table_size;
}
/*
 * FM-Index Operators
 */
// Compute SA[i]
GEM_INLINE uint64_t fm_index_lookup(const fm_index_t* const fm_index,const uint64_t bwt_position) {
  // TODO

//#ifndef SUPPRESS_CHECKS
//  gem_cond_fatal_error(_i>=a->bwt->n,BWT_INDEX,_i);
//#endif
//  register idx_t dist=0,i,next=_i;
//  bool is_sampled;
//  do {
//    i = next;
//    next=bwt_sampled_LF_bit__erank(a->bwt,i,&is_sampled);
//    ++dist;
//  } while (!is_sampled);
//#ifndef SUPPRESS_CHECKS
//  assert(dist<=(1<<a->sampling_rate_log));
//#endif
//  i = bwt_sampled_LF_erank(a->bwt,i);
//  GEM_UNALIGNED_ACCESS(i,a->D,a->cntr_bytes,i);
//  return (i + dist - 1) % (a->text_length+1); // Why not a-bwt-n?

  return 0;
}
// Compute SA^(-1)[i]
GEM_INLINE uint64_t fm_index_inverse_lookup(const fm_index_t* const fm_index,const uint64_t text_position) {
  // TODO


//#ifndef SUPPRESS_CHECKS
//  gem_cond_fatal_error(i<0||i>a->text_length,BWT_INDEX,i);
//#endif
//  register const idx_t refl=a->text_length-i;
//  register const idx_t sam_quo=GEM_DIV_BY_POWER_OF_TWO_64(refl,a->sampling_rate_log);
//  register idx_t sam_rem=GEM_MOD_BY_POWER_OF_TWO_64(refl,a->sampling_rate_log), pos;
//  GEM_UNALIGNED_ACCESS(pos,a->I,a->cntr_bytes,sam_quo);
//  while (sam_rem--)
//    pos=bwt_LF(a->bwt,pos);
//  return pos;

  return 0;
}
// Compute Psi[i]
GEM_INLINE uint64_t fm_index_psi(const fm_index_t* const fm_index,const uint64_t bwt_position) {
  // TODO

  // Pseudocode for a possible implementation follows, please do NOT delete it:
  /*
  if (!i)
    return a->last;
  --i;
  register int c=0;
  while (a->C[c+1]<=i)
    ++c;
  i-=a->C[c];
  register idx_t l=0,m,res=a->bwt->n;
  while (l<res) {
    m=(l+res)/2;
    if (i<fm_rankc(a,c,m))
      res=m;
    else
      l=m+1;
  }
  return res;
  */


  // return fmi_inverse(a,(fmi_lookup(a,i)+1)%a->bwt->n);
  return 0;
}
// Decode fm_index->text[bwt_position..bwt_position+length-1] into @buffer.
GEM_INLINE uint64_t fm_index_decode(
    const fm_index_t* const fm_index,const uint64_t bwt_position,const uint64_t length,char* const buffer) {
  // TODO

//#ifndef SUPPRESS_CHECKS
//  gem_cond_fatal_error(i<0||i>a->text_length,BWT_INDEX,i);
//  gem_cond_fatal_error(len<0||len>a->text_length,BWT_LEN,len);
//  gem_cond_fatal_error(i+len>a->text_length,PARAM_INV);
//#endif
//  if (__builtin_expect(a->bed!=0,1)) {
//    return be_decode(a->bed,i,len,p);
//  } else {
//    register idx_t pos=fmi_inverse(a,i+len),j;
//    for (j=0;j<len;++j) {
//      register const ch_t c=bwt_char(a->bwt,pos);
//      p[len-1-j]=c;
//      if (__builtin_expect(c==CHAR_ENC_SEP||c==CHAR_ENC_EOT,false))
//        p[len-1-j]=0;
//      pos=bwt_LF(a->bwt,pos);
//    }
//    p[len]=0;
//    return len;
//  }
//
  return 0;
}
GEM_INLINE uint64_t fm_index_decode_raw(
    const fm_index_t* const fm_index,const uint64_t bwt_position,const uint64_t length,char* const buffer) {
  // TODO


//#ifndef SUPPRESS_CHECKS
//  gem_cond_fatal_error(i<0||i>a->text_length,BWT_INDEX,i);
//  gem_cond_fatal_error(len<0||len>a->text_length,BWT_LEN,len);
//  gem_cond_fatal_error(i+len>a->text_length,PARAM_INV);
//#endif
//  if (__builtin_expect(a->bed!=0,1)) {
//    return be_decode_raw(a->bed,i,len,p);
//  } else {
//    register idx_t pos=fmi_inverse(a,i+len),j;
//    for (j=0;j<len;++j) {
//      p[len-1-j] = bwt_char(a->bwt,pos);
//      pos=bwt_LF(a->bwt,pos);
//    }
//    p[len]=0;
//    return len;
//  }

  return 0;
}
// Basic backward search
GEM_INLINE void fm_index_bsearch(
    const fm_index_t* const fm_index,
    const uint8_t* const key,uint64_t key_length,
    uint64_t* const hi_out,uint64_t* const lo_out) {
  FM_INDEX_CHECK(fm_index);
  GEM_CHECK_NULL(key);
  GEM_CHECK_POSITIVE(key_length);
  GEM_CHECK_NULL(hi_out); GEM_CHECK_NULL(lo_out);
  // Rank queries against the lookup table
  rank_mquery_t query;
  rank_mquery_new(&query);
  while (key_length > 0 && !rank_mquery_is_exhausted(&query)) {
    rank_mquery_add_char(&query,key[--key_length]); // Rank query (calculate offsets)
  }
  // Query lookup table
  uint64_t lo, hi;
  rank_mtable_fetch(fm_index->rank_table,&query,&lo,&hi);
  // Check zero interval
  if (hi==lo || key_length==0) {
    *hi_out=hi;
    *lo_out=lo;
  } else {
    // Continue with ranks against the FM-Index
    while (key_length > 0 && hi > lo) {
      const uint8_t c = key[--key_length];
      lo = bwt_erank(fm_index->bwt,c,lo);
      hi = bwt_erank(fm_index->bwt,c,hi);
    }
    // Return results
    *hi_out=hi;
    *lo_out=lo;
  }
}
/*
 * Extends the exact search of a key from a given position and interval
 */
GEM_INLINE uint64_t fm_index_bsearch_continue(
    const fm_index_t* const fm_index,
    const char* const key,const uint64_t key_length,
    const bool* const allowed_repl,
    uint64_t last_hi,uint64_t last_lo,
    uint64_t begin_pos,const uint64_t end_pos,
    uint64_t* const res_hi,uint64_t* const res_lo) {
//  #define RETURN_RESULT(lo_value, hi_value, end_pos) *res_lo=lo_value; *res_hi=hi_value; return end_pos
//  while (begin_pos>end_pos) {
//    if (last_lo==last_hi) {
//      RETURN_RESULT(last_lo, last_hi, begin_pos);
//    }
//
//    // Rank query
//    register const slch_t c = bwt_dna_p_encode[key[--begin_pos]];
//    if (!allowed_repl[c]) {
//      RETURN_RESULT(last_lo, last_hi, begin_pos+1);
//    }
//    last_lo=bwt_LF_erank_encoded(a->bwt,c,last_lo);
//    last_hi=bwt_LF_erank_encoded(a->bwt,c,last_hi);
//  }
//
//  RETURN_RESULT(last_lo, last_hi, begin_pos);
  return 0;
}
/*
 * Display
 */
GEM_INLINE void fm_index_print(FILE* const stream,const fm_index_t* const fm_index) {
  // Calculate some figures
  const uint64_t sampled_sa_size = sampled_sa_get_size(fm_index->sampled_sa); // Sampled SuffixArray positions
  const uint64_t rank_table_size = rank_mtable_get_size(fm_index->rank_table); // Memoizated intervals
  const uint64_t bwt_size = bwt_get_size(fm_index->bwt); // BWT structure
  const uint64_t fm_index_size = sampled_sa_size+bwt_size+rank_table_size;
  tab_fprintf(stream,"[GEM]>FM.Index\n");
  tab_fprintf(stream,"  => FM.Index.Size %lu MB\n",CONVERT_B_TO_MB(fm_index_size));
  tab_fprintf(stream,"    => Sampled.SA %lu MB (%2.3f%%)\n",CONVERT_B_TO_MB(sampled_sa_size),PERCENTAGE(sampled_sa_size,fm_index_size));
  tab_fprintf(stream,"    => Rank.mTable %lu MB (%2.3f%%)\n",CONVERT_B_TO_MB(rank_table_size),PERCENTAGE(rank_table_size,fm_index_size));
  tab_fprintf(stream,"    => BWT %lu MB (%2.3f%%)\n",CONVERT_B_TO_MB(bwt_size),PERCENTAGE(bwt_size,fm_index_size));
  tab_global_inc();
  // Sampled SuffixArray positions
  sampled_sa_print(stream,fm_index->sampled_sa);
  // Memoizated intervals
  rank_mtable_print(stream,fm_index->rank_table);
  // BWT structure
  bwt_print(stream,fm_index->bwt);
  tab_global_dec();
  // Flush
  fflush(stream);
}