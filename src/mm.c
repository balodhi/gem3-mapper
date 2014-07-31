/*
 * PROJECT: GEMMapper
 * FILE: mm.c
 * DATE: 06/06/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION:
 *   Memory Manager provides memory allocation functions. Different types of memory are supported.
 *     - UnitMemory
 *         Allocate relative small chunks of memory relying on the regular memory manager,
 *         usually malloc/calloc using a BuddySystem (Helper functions)
 *     - BulkMemory
 *         Allocate big chunks of memory and resort to disk if memory is not enough
 *     - SlabMemory
 *         Relative big amounts of objects allocated all at once (like the LINUX slab allocator)
 *         Objects of a certain type are ready to go inside the slab, thus reducing
 *         the overhead of malloc/setup/free cycles along the program
 *     - PoolMemory
 *         Pool of Slabs as gather all slabs needed along a program
 *         The goal is to minimize all memory malloc/setup/free overhead
 *         Offers thread safe allocation of slabs as to balance memory consumption across threads
 *  CONSIDERATIONS OF PERFORMANCE:
 *    1.- Use of other undelying memory managers for unit memory
 *      TCMalloc (Thread-Caching Malloc)
 *      nedmalloc
 *    2.- Use of hints ...
 *      madvise() / readahead() / posix_fadvise()
 */

#include "mm.h"
#include "fm.h"

#include <sys/resource.h>

#ifndef MAP_HUGETLB
  #define MAP_HUGETLB 0 // In some environments MAP_HUGETLB can be undefined
#endif
#ifndef MAP_ANONYMOUS
  #define MAP_ANONYMOUS 0 // Disabled for Mac compatibility
#endif
#ifndef MAP_POPULATE
  #define MAP_POPULATE 0 // Disabled for Mac compatibility
#endif

/*
 * Memory Alignment Utils
 */
const uint64_t mm_mem_alignment_bits_mask[] = { // Check Memory Alignment Bits (Masks)
    0x0000000000000001lu, /*    16 bits aligned (  2B / 2^4)  */
    0x0000000000000003lu, /*    32 bits aligned (  4B / 2^5)  */
    0x0000000000000007lu, /*    64 bits aligned (  8B / 2^6)  */
    0x000000000000000Flu, /*   128 bits aligned ( 16B / 2^7)  */
    0x000000000000001Flu, /*   256 bits aligned ( 32B / 2^8)  */
    0x000000000000003Flu, /*   512 bits aligned ( 64B / 2^9)  */
    0x000000000000007Flu, /*  1024 bits aligned ( 1KB / 2^10) */
    0x00000000000000FFlu, /*  2048 bits aligned ( 2KB / 2^11) */
    0x00000000000001FFlu, /*  4096 bits aligned ( 4KB / 2^12) RegularPage Size*/
    0x00000000000003FFlu, /*  8192 bits aligned ( 8KB / 2^13) */
    0x00000000000007FFlu, /* 16384 bits aligned (16KB / 2^14) */
    0x0000000000000FFFlu, /* 32768 bits aligned (32KB / 2^15) */
    0x000000000003FFFFlu, /*   n/a bits aligned ( 2MB / 2^21) RegularPageHugeTLB Size */
    0x000000000007FFFFlu, /*   n/a bits aligned ( 4MB / 2^21) */
};

/*
 * MMap Constants/Values
 */
int mm_proc_flags[3] = { PROT_READ, PROT_READ|PROT_WRITE, PROT_READ|PROT_WRITE };
int mm_mmap_mode[3] = { MAP_PRIVATE, MAP_SHARED, MAP_SHARED };
/*
 * Temporal folder path
 */
char* mm_temp_folder_path = MM_DEFAULT_TMP_FOLDER;

GEM_INLINE char* mm_get_tmp_folder() {
  return mm_temp_folder_path;
}
GEM_INLINE void mm_set_tmp_folder(char* const tmp_folder_path) {
  GEM_CHECK_NULL(tmp_folder_path);
  mm_temp_folder_path = tmp_folder_path;
}
/*
 * UnitMemory
 *   Allocate relative small chunks of memory relying on the regular memory manager,
 *   usually malloc/calloc using a BuddySystem (Helper functions)
 */
GEM_INLINE void* mm_malloc_nothrow(uint64_t const num_elements,const uint64_t size_element,const bool init_mem,const int init_value) {
  // Request memory
  const uint64_t total_memory = num_elements*size_element;
  void* const allocated_mem = (gem_expect_false(init_mem && init_value==0)) ?
      calloc(num_elements,size_element) : malloc(total_memory);
  // Check memory
  if (!allocated_mem) return NULL;
  // Initialize memory
  if (gem_expect_false(init_mem && init_value!=0)) memset(allocated_mem,init_value,total_memory);
  // MM_PRINT_MEM_ALIGMENT(allocated_mem); // Debug
  return allocated_mem;
}
GEM_INLINE void* mm_malloc_(uint64_t const num_elements,const uint64_t size_element,const bool init_mem,const int init_value) {
  // Request memory
  const uint64_t total_memory = num_elements*size_element;
  void* allocated_mem;
  if (gem_expect_false(init_mem && init_value==0)) {
    allocated_mem = calloc(num_elements,size_element);
    gem_cond_fatal_error(!allocated_mem,MEM_CALLOC,num_elements,size_element);
  } else {
    allocated_mem = malloc(total_memory);
    gem_cond_fatal_error(!allocated_mem,MEM_ALLOC,total_memory);
  }
  // Initialize memory
  if (gem_expect_false(init_mem && init_value!=0)) memset(allocated_mem,init_value,total_memory);
  // MM_PRINT_MEM_ALIGMENT(allocated_mem); // Debug
  return allocated_mem;
}
GEM_INLINE void* mm_realloc_nothrow(void* mem_addr,const uint64_t num_bytes) {
  GEM_CHECK_NULL(mem_addr);
  GEM_CHECK_ZERO(num_bytes);
  return realloc(mem_addr,num_bytes);
}
GEM_INLINE void* mm_realloc(void* mem_addr,const uint64_t num_bytes) {
  GEM_CHECK_NULL(mem_addr);
  GEM_CHECK_ZERO(num_bytes);
  void* const new_mem_addr = mm_realloc_nothrow(mem_addr,num_bytes);
  gem_cond_fatal_error(!new_mem_addr,MEM_REALLOC,num_bytes);
  return new_mem_addr;
}
GEM_INLINE void mm_free(void* mem_addr) {
  GEM_CHECK_NULL(mem_addr);
  free(mem_addr);
}
/*
 * BulkMemory
 *   Allocate big chunks of memory and resort to disk if memory is not enough
 */
GEM_INLINE mm_t* mm_bulk_malloc(const uint64_t num_bytes,const bool init_mem) {
  GEM_CHECK_ZERO(num_bytes);
  void* memory = mm_malloc_nothrow(num_bytes,1,init_mem,0);
  if (gem_expect_true(memory!=NULL)) { // Fits in HEAP
    mm_t* const mem_manager = mm_alloc(mm_t);
    mem_manager->memory = memory;
    mem_manager->mem_type = MM_HEAP;
    mem_manager->mode = MM_READ_WRITE;
    mem_manager->allocated = num_bytes;
    mem_manager->cursor = mem_manager->memory;
    // MM_PRINT_MEM_ALIGMENT(mem_manager->memory); // Debug
    return mem_manager;
  } else { // Resort to MMAP in disk
    gem_warn(MEM_ALLOC_DISK,num_bytes);
    return mm_bulk_mmalloc_temp(num_bytes);
  }
}
GEM_INLINE mm_t* mm_bulk_mmalloc(const uint64_t num_bytes,const bool use_huge_pages) {
  GEM_CHECK_ZERO(num_bytes);
  // Allocate handler
  mm_t* const mem_manager = mm_alloc(mm_t);
  /*
   * MMap memory (anonymous)
   *   - MAP_PRIVATE => Fits in RAM+SWAP
   *   - MAP_ANONYMOUS => The mapping is not backed by any file; its contents are initialized to zero.
   *       Map against /dev/zero (Allocate anonymous memory segment, without open)
   *   - MAP_NORESERVE to explicitly enable swap space overcommitting. (echo 1 > /proc/sys/vm/overcommit_memory)
   *     Useful when you wish to map a file larger than the amount of free memory
   *     available on your system (RAM+SWAP).
   *       In this case, the lazy swap space reservation may cause the program
   *       to consume all the free RAM and swap on the system, eventually
   *       triggering the OOM killer (Linux) or causing a SIGSEGV.
   */
  int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
  if (use_huge_pages) flags |= MAP_HUGETLB;
  mem_manager->memory = mmap(0,num_bytes,PROT_READ|PROT_WRITE,flags,-1,0);
  gem_cond_fatal_error__perror(mem_manager->memory==MAP_FAILED,MEM_ALLOC_MMAP_FAIL,num_bytes);
  mem_manager->cursor = mem_manager->memory;
  // Set MM
  mem_manager->mem_type = MM_MMAPPED;
  mem_manager->mode = MM_READ_WRITE;
  mem_manager->allocated = num_bytes;
  mem_manager->fd = -1;
  mem_manager->file_name = NULL;
  // MM_PRINT_MEM_ALIGMENT(mem_manager->memory); // Debug
  return mem_manager;
}
GEM_INLINE mm_t* mm_bulk_mmalloc_temp(const uint64_t num_bytes) {
  GEM_CHECK_ZERO(num_bytes);
  // Allocate handler
  mm_t* const mem_manager = mm_alloc(mm_t);
  // TemporalMemory (backed by a file)
  mem_manager->file_name = mm_calloc(strlen(mm_get_tmp_folder())+22,char,true);
  sprintf(mem_manager->file_name,"%smm_temp_XXXXXX",mm_get_tmp_folder());
  // Create temporary file
  mem_manager->fd = mkstemp(mem_manager->file_name);
  gem_cond_fatal_error__perror(mem_manager->fd==-1,SYS_MKSTEMP,mem_manager->file_name);
  gem_cond_fatal_error__perror(unlink(mem_manager->file_name),SYS_HANDLE_TMP); // Make it temporary
  gem_log("Allocating memory mapped to disk: %s (%lu MBytes) [PhysicalMem Available %lu MBytes]",
      mem_manager->file_name,num_bytes/1024/1024,mm_get_available_mem()/1024/1024);
  // Set the size of the temporary file (disk allocation)
  gem_cond_fatal_error__perror(lseek(mem_manager->fd,num_bytes-1,SEEK_SET)==-1,SYS_HANDLE_TMP);
  gem_cond_fatal_error__perror(write(mem_manager->fd,"",1)<=0,SYS_HANDLE_TMP);
  gem_cond_fatal_error__perror(lseek(mem_manager->fd,0,SEEK_SET)==-1,SYS_HANDLE_TMP);
  /*
   * Mmap file.
   *   - MAP_SHARED as we the mapping will be reflected on disk (no copy-on-write)
   *     As such, the kernel knows it can always free up memory by doing writeback.
   *   - MAP_NORESERVE to explicitly enable swap space overcommitting. (echo 1 > /proc/sys/vm/overcommit_memory)
   *     Useful when you wish to map a file larger than the amount of free memory
   *     available on your system (RAM+SWAP).
   *       In this case, the lazy swap space reservation may cause the program
   *       to consume all the free RAM and swap on the system, eventually
   *       triggering the OOM killer (Linux) or causing a SIGSEGV.
   */
  mem_manager->memory = mmap(NULL,num_bytes,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_NORESERVE,mem_manager->fd,0);
  gem_cond_fatal_error__perror(mem_manager->memory==MAP_FAILED,MEM_ALLOC_MMAP_DISK_FAIL,num_bytes,mem_manager->file_name);
  mem_manager->cursor = mem_manager->memory;
  // Set MM
  mem_manager->mem_type = MM_MMAPPED;
  mem_manager->mode = MM_READ_WRITE;
  mem_manager->allocated = num_bytes;
  // MM_PRINT_MEM_ALIGMENT(mem_manager->memory); // Debug
  return mem_manager;
}
GEM_INLINE void mm_bulk_free(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  if (mem_manager->mem_type==MM_HEAP) { // Heap BulkMemory
    mm_free(mem_manager->memory);
  } else { // MMapped BulkMemory
    gem_cond_fatal_error__perror(munmap(mem_manager->memory,mem_manager->allocated)==-1,SYS_UNMAP);
    if (mem_manager->fd!=-1) {
      gem_cond_fatal_error__perror(close(mem_manager->fd),SYS_HANDLE_TMP);
    }
  }
  CFREE(mem_manager->file_name);
  mm_free(mem_manager);
}
GEM_INLINE mm_t* mm_bulk_mmap_file(char* const file_name,const mm_mode mode,const bool populate_page_tables) {
  GEM_CHECK_NULL(file_name);
  // Allocate handler
  mm_t* const mem_manager = mm_alloc(mm_t);
  // Retrieve input file info
  struct stat stat_info;
  gem_stat(file_name,&stat_info);
  gem_cond_fatal_error(stat_info.st_size==0,MEM_MAP_ZERO_FILE,file_name);
  // Open file descriptor
  mem_manager->fd = gem_open_fd(file_name,fm_open_flags[mode],S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  /*
   * Mmap file
   *   - @mode::
   *       MM_READ_ONLY => MAP_PRIVATE (no copy-on-write as it's not allowed)
   *       MM_WRITE_ONLY or MM_READ_WRITE => MAP_SHARED
   *   - MAP_POPULATE (since Linux 2.5.46)
   *       Populate (prefault) page tables for a mapping. For a file mapping, this causes
   *       read-ahead on the file. Later accesses to the mapping will not be blocked by page faults.
   *       MAP_POPULATE is only supported for private mappings since Linux 2.6.23.
   */
  int flags = mm_mmap_mode[mode];
  if (mode==MM_READ_ONLY && populate_page_tables) flags |= MAP_POPULATE;
  mem_manager->memory = mmap(0,stat_info.st_size,mm_proc_flags[mode],flags,mem_manager->fd,0);
  gem_cond_fatal_error__perror(mem_manager->memory==MAP_FAILED,SYS_MMAP_FILE,file_name);
  mem_manager->cursor = mem_manager->memory;
  // Set MM
  mem_manager->mem_type = MM_MMAPPED;
  mem_manager->mode = mode;
  mem_manager->allocated = stat_info.st_size;
  mem_manager->file_name = strndup(file_name,strlen(file_name));
  // MM_PRINT_MEM_ALIGMENT(mem_manager->memory); // Debug
  return mem_manager;
}
GEM_INLINE mm_t* mm_bulk_load_file(char* const file_name,const uint64_t num_threads) {
  GEM_CHECK_NULL(file_name);
  // Allocate handler
  mm_t* const mem_manager = mm_alloc(mm_t);
  // Retrieve input file info
  struct stat stat_info;
  gem_cond_fatal_error__perror(stat(file_name,&stat_info)==-1,SYS_STAT,file_name);
  gem_cond_fatal_error(stat_info.st_size==0,MEM_MAP_ZERO_FILE,file_name);
  // Allocate memory to dump the content of the file
  mem_manager->memory = mm_malloc(stat_info.st_size);
  gem_cond_fatal_error(!mem_manager->memory,MEM_ALLOC,stat_info.st_size);
  mem_manager->mem_type = MM_HEAP;
  mem_manager->mode = MM_READ_ONLY;
  mem_manager->allocated = stat_info.st_size;
  mem_manager->cursor = mem_manager->memory;
  // MM_PRINT_MEM_ALIGMENT(mem_manager->memory); // Debug
  // Read the file and dump it into memory
  if (num_threads>1 && (stat_info.st_size > num_threads*8)) {
    fm_bulk_read_file_parallel(file_name,mem_manager->memory,0,0,num_threads);
  } else {
    fm_bulk_read_file(file_name,mem_manager->memory,0,0);
  }
  return mem_manager;
}
GEM_INLINE mm_t* mm_bulk_mload_file(char* const file_name,const uint64_t num_threads) {
  GEM_CHECK_NULL(file_name);
  // Retrieve input file info
  struct stat stat_info;
  gem_cond_fatal_error__perror(stat(file_name,&stat_info)==-1,SYS_STAT,file_name);
  gem_cond_fatal_error(stat_info.st_size==0,MEM_MAP_ZERO_FILE,file_name);
  // Allocate memory to dump the content of the file
  mm_t* const mem_manager = mm_bulk_mmalloc(stat_info.st_size,false);
  // Read the file and dump it into memory
  if (num_threads>1 && (stat_info.st_size > num_threads*8)) {
    fm_bulk_read_file_parallel(file_name,mem_manager->memory,0,0,num_threads);
  } else {
    fm_bulk_read_file(file_name,mem_manager->memory,0,0);
  }
  return mem_manager;
}

/*
 * Accessors
 */
GEM_INLINE void* mm_get_mem(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return mem_manager->cursor;
}
GEM_INLINE void* mm_get_base_mem(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return mem_manager->memory;
}
GEM_INLINE mm_mode mm_get_mode(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return mem_manager->mode;
}
GEM_INLINE uint64_t mm_get_allocated(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return mem_manager->allocated;
}
GEM_INLINE char* mm_get_mfile_name(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return mem_manager->file_name;
}
/*
 * Seek functions
 */
GEM_INLINE uint64_t mm_get_current_position(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return (mem_manager->cursor-mem_manager->memory);
}
GEM_INLINE bool mm_eom(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  return mm_get_current_position(mem_manager) >= mem_manager->allocated;
}
GEM_INLINE void mm_seek(mm_t* const mem_manager,const uint64_t byte_position) {
  MM_CHECK(mem_manager);
  gem_fatal_check(byte_position>=mem_manager->allocated,MEM_CURSOR_SEEK,byte_position);
  mem_manager->cursor = mem_manager->memory + byte_position;
}
GEM_INLINE void mm_skip_forward(mm_t* const mem_manager,const uint64_t num_bytes) {
  MM_CHECK(mem_manager);
  mem_manager->cursor += num_bytes;
  MM_CHECK_SEGMENT(mem_manager);
}
GEM_INLINE void mm_skip_backward(mm_t* const mem_manager,const uint64_t num_bytes) {
  MM_CHECK(mem_manager);
  mem_manager->cursor -= num_bytes;
  MM_CHECK_SEGMENT(mem_manager);
}
GEM_INLINE void mm_skip_uint64(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor += 8;
  MM_CHECK_SEGMENT(mem_manager);
}
GEM_INLINE void mm_skip_uint32(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor += 4;
  MM_CHECK_SEGMENT(mem_manager);
}
GEM_INLINE void mm_skip_uint16(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor += 2;
  MM_CHECK_SEGMENT(mem_manager);
}
GEM_INLINE void mm_skip_uint8(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor += 1;
  MM_CHECK_SEGMENT(mem_manager);
}
GEM_INLINE void mm_skip_align(mm_t* const mem_manager,const uint64_t num_bytes) {
  MM_CHECK(mem_manager);
  GEM_CHECK_ZERO(num_bytes);
  if (gem_expect_true(num_bytes > 1)) {
    mem_manager->cursor = mem_manager->cursor+(num_bytes-1);
    mem_manager->cursor = mem_manager->cursor-(MM_CAST_ADDR(mem_manager->cursor)%num_bytes);
    MM_CHECK_SEGMENT(mem_manager);
    gem_fatal_check(MM_CAST_ADDR(mem_manager->cursor)%num_bytes!=0,MEM_ALG_FAILED);
  }
}
GEM_INLINE void mm_skip_align_16(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_16b) & (~MM_MEM_ALIGNED_MASK_16b));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,16b);
}
GEM_INLINE void mm_skip_align_32(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_32b) & (~MM_MEM_ALIGNED_MASK_32b));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,32b);
}
GEM_INLINE void mm_skip_align_64(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_64b) & (~MM_MEM_ALIGNED_MASK_64b));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,64b);
}
GEM_INLINE void mm_skip_align_128(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_128b) & (~MM_MEM_ALIGNED_MASK_128b));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,128b);
}
GEM_INLINE void mm_skip_align_512(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_512b) & (~MM_MEM_ALIGNED_MASK_512b));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,512b);
}
GEM_INLINE void mm_skip_align_1024(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_1KB) & (~MM_MEM_ALIGNED_MASK_1KB));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,1KB);
}
GEM_INLINE void mm_skip_align_4KB(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  mem_manager->cursor = MM_CAST_PTR(
      (MM_CAST_ADDR(mem_manager->cursor)+MM_MEM_ALIGNED_MASK_4KB) & (~MM_MEM_ALIGNED_MASK_4KB));
  MM_CHECK_SEGMENT(mem_manager);
  MM_CHECK_ALIGNMENT(mem_manager,4KB);
}
GEM_INLINE void mm_skip_align_mempage(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  const int64_t page_size = mm_get_page_size();
  gem_cond_fatal_error__perror(page_size==-1,SYS_SYSCONF);
  mm_skip_align(mem_manager,page_size);
}

/*
 * Read functions
 */
GEM_INLINE uint64_t mm_read_uint64(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  const uint64_t data = *((uint64_t*)mem_manager->cursor);
  mem_manager->cursor += 8;
  return data;
}
GEM_INLINE uint32_t mm_read_uint32(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  const uint32_t data = *((uint32_t*)mem_manager->cursor);
  mem_manager->cursor += 4;
  return data;
}
GEM_INLINE uint16_t mm_read_uint16(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  const uint16_t data = *((uint16_t*)mem_manager->cursor);
  mem_manager->cursor += 2;
  return data;
}
GEM_INLINE uint8_t mm_read_uint8(mm_t* const mem_manager) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  const uint8_t data = *((uint8_t*)mem_manager->cursor);
  mem_manager->cursor += 1;
  return data;
}
GEM_INLINE void* mm_read_mem(mm_t* const mem_manager,const uint64_t num_bytes) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  void* const current_cursor = mem_manager->cursor;
  mem_manager->cursor += num_bytes;
  return current_cursor;
}
GEM_INLINE void mm_copy_mem(mm_t* const mem_manager,void* const dst,const uint64_t num_bytes) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  memcpy(dst,mem_manager->cursor,num_bytes);
  mem_manager->cursor += num_bytes;
}
GEM_INLINE void mm_copy_mem_parallel(mm_t* const mem_manager,void* const dst,const uint64_t num_bytes,const uint64_t num_threads) {
//  TODO
}

/*
 * Write functions
 */
GEM_INLINE void mm_write_uint64(mm_t* const mem_manager,const uint64_t data) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  *((uint64_t*)mem_manager->cursor) = data;
  mem_manager->cursor += 8;
}
GEM_INLINE void mm_write_uint32(mm_t* const mem_manager,const uint32_t data) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  *((uint32_t*)mem_manager->cursor) = data;
  mem_manager->cursor += 4;
}
GEM_INLINE void mm_write_uint16(mm_t* const mem_manager,const uint16_t data) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  *((uint16_t*)mem_manager->cursor) = data;
  mem_manager->cursor += 2;
}
GEM_INLINE void mm_write_uint8(mm_t* const mem_manager,const uint8_t data) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  *((uint8_t*)mem_manager->cursor) = data;
  mem_manager->cursor += 1;
}
GEM_INLINE void mm_write_mem(mm_t* const mem_manager,void* const src,const uint64_t num_bytes) {
  MM_CHECK(mem_manager);
  MM_CHECK_SEGMENT(mem_manager);
  memcpy(mem_manager->cursor,src,num_bytes);
  mem_manager->cursor += num_bytes;
}

/*
 * Status
 */
GEM_INLINE int64_t mm_get_page_size() {
  int64_t page_size = sysconf(_SC_PAGESIZE);
  gem_cond_fatal_error__perror(page_size==-1,SYS_SYSCONF);
  return page_size; // Bytes
}
GEM_INLINE int64_t mm_get_stat_meminfo(const char* const label,const uint64_t label_length) {
  // Open /proc/meminfo
  FILE* const meminfo = fopen("/proc/meminfo", "r");
  gem_cond_fatal_error__perror(meminfo == NULL,MEM_STAT_MEMINFO,"no such file");
  // Parse /proc/meminfo
  char *line = NULL;
  uint64_t size=0,line_length=0,chars_read=0;
  while ((chars_read=getline(&line,&line_length,meminfo))!=-1) {
    if (strncmp(line,"Cached:",7)==0) {
      sscanf(line,"%*s %lu",&size);
      free(line); fclose(meminfo);
      return size*1024; // Bytes
    }
  }
  // Not found
  free(line); fclose(meminfo);
  return -1;
}
GEM_INLINE int64_t mm_get_available_cached_mem() {
  const int64_t size = mm_get_stat_meminfo("Cached:",7);
  gem_cond_fatal_error__perror(size==-1,MEM_STAT_MEMINFO,"Cached");
  return size;
}
GEM_INLINE int64_t mm_get_available_free_mem() {
  const int64_t size = mm_get_stat_meminfo("MemFree:",8);
  gem_cond_fatal_error__perror(size==-1,MEM_STAT_MEMINFO,"MemFree");
  return size;
}
GEM_INLINE int64_t mm_get_available_mem() {
  return mm_get_available_free_mem()+mm_get_available_cached_mem();
}
GEM_INLINE int64_t mm_get_available_virtual_mem() {
  // Get Total Program Size
  uint64_t vm_size = 0;
  FILE *statm = fopen("/proc/self/statm", "r");
  gem_cond_fatal_error(fscanf(statm,"%ld", &vm_size)==-1,MEM_PARSE_STATM);
  vm_size = (vm_size + 1) * 1024;
  fclose(statm);
  // Get Virtual Memory Limit
  struct rlimit lim;
  getrlimit(RLIMIT_AS,&lim);
  // Return Virtual Memory Available
  return lim.rlim_cur - vm_size; // Bytes
}
