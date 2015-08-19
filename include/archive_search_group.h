/*
 * PROJECT: GEMMapper
 * FILE: archive_search_group.h
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: // TODO
 */

#ifndef ARCHIVE_SEARCH_GROUP_H_
#define ARCHIVE_SEARCH_GROUP_H_

#include "essentials.h"
#include "archive_search.h"
#include "bpm_align_gpu.h"
#include "buffered_output_file.h"
#include "mapper.h"

/*
 * Archive-Search groups
 */
typedef struct _archive_search_group_t archive_search_group_t;

/*
 * Archive-Search group
 */
archive_search_group_t* archive_search_group_new(
    mapper_parameters_t* const mapper_parameters,bpm_gpu_buffer_t* const bpm_gpu_buffers,const uint64_t total_search_groups);
void archive_search_group_init_bpm_buffers(archive_search_group_t* const archive_search_group);
void archive_search_group_clear(archive_search_group_t* const archive_search_group);
void archive_search_group_delete(archive_search_group_t* const archive_search_group);

archive_search_t* archive_search_group_allocate(archive_search_group_t* const archive_search_group);
void archive_search_group_allocate_pe(
    archive_search_group_t* const archive_search_group,
    archive_search_t** const archive_search_end1,archive_search_t** const archive_search_end2);

bool archive_search_group_is_empty(archive_search_group_t* const archive_search_group);

bool archive_search_group_add_search(
    archive_search_group_t* const archive_search_group,archive_search_t* const archive_search);
bool archive_search_group_add_paired_search(
    archive_search_group_t* const archive_search_group,
    archive_search_t* const archive_search_end1,archive_search_t* const archive_search_end2);
void archive_search_group_retrieve_begin(archive_search_group_t* const archive_search_group);
bool archive_search_group_get_search(
    archive_search_group_t* const archive_search_group,
    archive_search_t** const archive_search,bpm_gpu_buffer_t** const bpm_gpu_buffer);
bool archive_search_group_get_paired_search(
    archive_search_group_t* const archive_search_group,
    archive_search_t** const archive_search_end1,bpm_gpu_buffer_t** const bpm_gpu_buffer_end1,
    archive_search_t** const archive_search_end2,bpm_gpu_buffer_t** const bpm_gpu_buffer_end2);

text_collection_t* archive_search_group_get_text_collection(archive_search_group_t* const archive_search_group);

/*
 * Error Messages
 */
#define GEM_ERROR_ARCHIVE_SEARCH_GROUP_QUERY_TOO_BIG "Archive-Search group. Couldn't copy query to BPM-buffer (Query too big)"
#define GEM_ERROR_ARCHIVE_SEARCH_GROUP_UNPAIRED_QUERY "Archive-Search group. Couldn't retrieve query-pair"

#endif /* ARCHIVE_SEARCH_GROUP_H_ */
