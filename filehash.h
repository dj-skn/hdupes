/* hdupes file hashing function
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_FILEHASH_H
#define HDUPES_FILEHASH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASH_ALGO_COUNT 2
extern const char *hash_algo_list[HASH_ALGO_COUNT];
#define HASH_ALGO_XXHASH2_64 0
#define HASH_ALGO_JODYHASH64 1

#include "hdupes.h"

uint64_t *get_filehash(const file_t * const restrict checkfile, const size_t max_read, int algo);
int compute_filehash_with_buffer(const file_t * const restrict checkfile,
                                 const size_t max_read,
                                 int algo,
                                 uint64_t *out_hash,
                                 void *buffer,
                                 size_t buffer_size,
                                 int allow_progress);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_FILEHASH_H */
