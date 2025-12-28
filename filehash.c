/* hdupes file hashing function
 * This file is part of hdupes; see hdupes.c for license information */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#include <libjodycode.h>

#include "likely_unlikely.h"
#include "filehash.h"
#include "interrupt.h"
#include "progress.h"
#include "hdupes.h"
#include "xxhash.h"

const char *hash_algo_list[2] = {
  "xxHash64 v2",
  "jodyhash v7"
};


/* Hash part or all of a file
 *
 *              READ THIS BEFORE CHANGING THE HASH FUNCTION!
 * The hash function is only used to do fast exclusion. There is not much
 * benefit to using bigger or "better" hash functions. Upstream hdupes WILL
 * NOT accept any pull requests that change the hash function unless there
 * is an EXTREMELY compelling reason to do so. Do not waste your time with
 * swapping hash functions. If you want to do it for fun then that's fine. */
int compute_filehash_with_buffer(const file_t * const restrict checkfile,
                                 const size_t max_read,
                                 int algo,
                                 uint64_t *out_hash,
                                 void *buffer,
                                 size_t buffer_size,
                                 int allow_progress)
{
  off_t fsize;
  uint64_t hash = 0;
  uint64_t *chunk;
  FILE *file = NULL;
  int hashing = 0;
#ifndef NO_XXHASH2
  XXH64_state_t *xxhstate = NULL;
#endif
#ifdef __linux__
  int filenum;
#endif

  if (unlikely(checkfile == NULL || checkfile->d_name == NULL || out_hash == NULL || buffer == NULL))
    jc_nullptr("compute_filehash_with_buffer()");
  if (unlikely((algo > HASH_ALGO_COUNT - 1) || (algo < 0))) goto error_bad_hash_algo;
  LOUD(fprintf(stderr, "get_filehash('%s', %" PRIdMAX ")\n", checkfile->d_name, (intmax_t)max_read);)

  if (unlikely(buffer_size < auto_chunk_size)) goto error_buffer_too_small;
  chunk = (uint64_t *)buffer;

  /* Get the file size. If we can't read it, bail out early */
  if (unlikely(checkfile->size == -1)) {
    LOUD(fprintf(stderr, "get_filehash: not hashing because stat() info is bad\n"));
    return -1;
  }
  fsize = checkfile->size;

  /* Do not read more than the requested number of bytes */
  if (max_read > 0 && fsize > (off_t)max_read)
    fsize = (off_t)max_read;

  /* Initialize the hash and file read parameters (with filehash_partial skipped)
   *
   * If we already hashed the first chunk of this file, we don't want to
   * wastefully read and hash it again, so skip the first chunk and use
   * the computed hash for that chunk as our starting point.
   */

  hash = 0;
  if (ISFLAG(checkfile->flags, FF_HASH_PARTIAL)) {
    hash = checkfile->filehash_partial;
    /* Don't bother going further if max_read is already fulfilled */
    if (max_read != 0 && max_read <= PARTIAL_HASH_SIZE) {
      LOUD(fprintf(stderr, "Partial hash size (%d) >= max_read (%" PRIuMAX "), not hashing anymore\n", PARTIAL_HASH_SIZE, (uintmax_t)max_read);)
      *out_hash = hash;
      return 0;
    }
  }
  errno = 0;
  file = jc_fopen(checkfile->d_name, JC_FILE_MODE_RDONLY_SEQ);
  if (file == NULL) goto error_opening_file;

  /* Actually seek past the first chunk if applicable
   * This is part of the filehash_partial skip optimization */
  if (ISFLAG(checkfile->flags, FF_HASH_PARTIAL)) {
    if (fseeko(file, PARTIAL_HASH_SIZE, SEEK_SET) == -1) goto error_seeking_file;
    fsize -= PARTIAL_HASH_SIZE;
#ifdef __linux__
    filenum = fileno(file);
    posix_fadvise(filenum, PARTIAL_HASH_SIZE, fsize, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(filenum, PARTIAL_HASH_SIZE, fsize, POSIX_FADV_WILLNEED);
#endif /* __linux__ */
  } else {
#ifdef __linux__
    filenum = fileno(file);
    posix_fadvise(filenum, 0, fsize, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(filenum, 0, fsize, POSIX_FADV_WILLNEED);
#endif /* __linux__ */
  }

/* WARNING: READ NOTICE ABOVE get_filehash() BEFORE CHANGING HASH FUNCTIONS! */
#ifndef NO_XXHASH2
  if (algo == HASH_ALGO_XXHASH2_64) {
    xxhstate = XXH64_createState();
    if (unlikely(xxhstate == NULL)) jc_nullptr("xxhstate");
    XXH64_reset(xxhstate, 0);
  }
#endif /* NO_XXHASH2 */

  /* Read the file in chunks until we've read it all. */
  while (fsize > 0) {
    size_t bytes_to_read;

    if (interrupt) goto error_interrupt;
    bytes_to_read = (fsize >= (off_t)auto_chunk_size) ? auto_chunk_size : (size_t)fsize;
    if (unlikely(fread((void *)chunk, bytes_to_read, 1, file) != 1)) goto error_reading_file;

  switch (algo) {
#ifndef NO_XXHASH2
    case HASH_ALGO_XXHASH2_64:
      if (unlikely(XXH64_update(xxhstate, chunk, bytes_to_read) != XXH_OK)) goto error_reading_file;
      break;
#endif
    case HASH_ALGO_JODYHASH64:
      if (unlikely(jc_block_hash(NORMAL, chunk, &hash, bytes_to_read) != 0)) goto error_reading_file;
      break;
    default:
      goto error_bad_hash_algo;
  }

    if ((off_t)bytes_to_read > fsize) break;
    else fsize -= (off_t)bytes_to_read;

    if (allow_progress) {
      check_sigusr1();
      if (jc_alarm_ring != 0) {
        jc_alarm_ring = 0;
        /* Only show "hashing" part if hashing one file updates progress at least twice */
        if (hashing == 1) {
          update_phase2_progress("hashing", (int)(((checkfile->size - fsize) * 100) / checkfile->size));
        } else {
          update_phase2_progress(NULL, -1);
          hashing = 1;
        }
      }
    }
    continue;
  }

  fclose(file);
  file = NULL;

#ifndef NO_XXHASH2
  if (algo == HASH_ALGO_XXHASH2_64) {
    hash = XXH64_digest(xxhstate);
    XXH64_freeState(xxhstate);
  }
#endif /* NO_XXHASH2 */

  LOUD(fprintf(stderr, "get_filehash: returning hash: 0x%016jx\n", (uintmax_t)hash));
  *out_hash = hash;
  return 0;
error_reading_file:
  fprintf(stderr, "\nerror reading from file "); jc_fwprint(stderr, checkfile->d_name, 1);
  if (file != NULL) fclose(file);
  return -1;
error_opening_file:
  fprintf(stderr, "\n%s error opening file ", strerror(errno)); jc_fwprint(stderr, checkfile->d_name, 1);
  if (file != NULL) fclose(file);
  return -1;
error_seeking_file:
  fprintf(stderr, "\nerror seeking in file "); jc_fwprint(stderr, checkfile->d_name, 1);
  if (file != NULL) fclose(file);
  return -1;
error_interrupt:
  if (file != NULL) fclose(file);
  return -1;
error_bad_hash_algo:
  if ((hash_algo > HASH_ALGO_COUNT) || (hash_algo < 0))
    fprintf(stderr, "\nerror: requested hash algorithm %d is not available", hash_algo);
  else
    fprintf(stderr, "\nerror: requested hash algorithm %s [%d] is not available", hash_algo_list[hash_algo], hash_algo);
  if (file != NULL) fclose(file);
  return -1;
error_buffer_too_small:
  fprintf(stderr, "\nerror: hash buffer too small");
  return -1;
}


uint64_t *get_filehash(const file_t * const restrict checkfile, const size_t max_read, int algo)
{
  /* This is an array because we return a pointer to it */
  static uint64_t hash[1];
  static uint64_t *chunk = NULL;

  /* Allocate on first use */
  if (unlikely(chunk == NULL)) {
    chunk = (uint64_t *)malloc(auto_chunk_size);
    if (unlikely(!chunk)) jc_oom("get_filehash() chunk");
  }

  if (compute_filehash_with_buffer(checkfile, max_read, algo, &hash[0], chunk, auto_chunk_size, 1) != 0)
    return NULL;

  return hash;
}
