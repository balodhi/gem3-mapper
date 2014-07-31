/*
 * PROJECT: GEMMapper
 * FILE: gem_core.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 */

#include "gem_core.h"

/*
 * GEM Runtime
 */
GEM_INLINE void gem_runtime_init(
    const uint64_t num_threads,const uint64_t max_memory,char* const tmp_folder,report_function_t report_function) {
  // GEM error handler
  gem_handle_error_signals();
  // Register Master-Thread
  gem_thread_register_id(0);
  // Setup Profiling
  PROF_NEW(num_threads+1); // Add master thread
  // Configure Memory Limits // TODO
  if (max_memory==0) {
    // f(mm_get_available_mem();
  } else {
    // f(max_memory);
  }
  // Setup temporal folder
  if (tmp_folder!=NULL) mm_set_tmp_folder(tmp_folder);
  // Configure report function
  if (report_function!=NULL) gem_error_set_report_function(report_function);
}
GEM_INLINE void gem_runtime_delete() {
  // Clean-up Profiler
  PROF_DELETE();
  // Delete Memory-Pool
  mm_pool_delete();
}