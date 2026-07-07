/* Experimental array/group matching pipeline
 * This file is part of hdupes; see hdupes.c for license information */

#include <stdint.h>
#include <stdlib.h>

#include <libjodycode.h>

#include "filehash.h"
#ifndef NO_HASHDB
 #include "hashdb.h"
#endif
#include "hdupes.h"
#include "checks.h"
#include "interrupt.h"
#include "match.h"
#include "pipeline.h"
#include "sort.h"

typedef struct pipeline_entry {
  file_t *file;
  uintmax_t index;
} pipeline_entry_t;

static int cmp_entry_size(const void *a, const void *b)
{
  const pipeline_entry_t *ea = (const pipeline_entry_t *)a;
  const pipeline_entry_t *eb = (const pipeline_entry_t *)b;

  if (ea->file->size < eb->file->size) return -1;
  if (ea->file->size > eb->file->size) return 1;
  if (ea->index < eb->index) return -1;
  if (ea->index > eb->index) return 1;
  return 0;
}

static int cmp_entry_full_hash(const void *a, const void *b)
{
  const pipeline_entry_t *ea = (const pipeline_entry_t *)a;
  const pipeline_entry_t *eb = (const pipeline_entry_t *)b;

  if (ea->file->filehash < eb->file->filehash) return -1;
  if (ea->file->filehash > eb->file->filehash) return 1;
  if (ea->index < eb->index) return -1;
  if (ea->index > eb->index) return 1;
  return 0;
}

static int ensure_pipeline_hashes(file_t *file)
{
  const uint64_t *filehash;
#ifndef NO_HASHDB
  int dirty = 0;
#endif

  if (!ISFLAG(file->flags, FF_HASH_PARTIAL)) {
    filehash = get_filehash(file, PARTIAL_HASH_SIZE, hash_algo);
    if (filehash == NULL) return -1;
    file->filehash_partial = *filehash;
    SETFLAG(file->flags, FF_HASH_PARTIAL);
#ifndef NO_HASHDB
    dirty = 1;
#endif
  }

  if (file->size <= PARTIAL_HASH_SIZE || ISFLAG(flags, F_PARTIALONLY)) {
    if (!ISFLAG(file->flags, FF_HASH_FULL)) {
      file->filehash = file->filehash_partial;
      SETFLAG(file->flags, FF_HASH_FULL);
#ifndef NO_HASHDB
      dirty = 1;
#endif
    }
  } else if (!ISFLAG(file->flags, FF_HASH_FULL)) {
    filehash = get_filehash(file, 0, hash_algo);
    if (filehash == NULL) return -1;
    file->filehash = *filehash;
    SETFLAG(file->flags, FF_HASH_FULL);
#ifndef NO_HASHDB
    dirty = 1;
#endif
  }

#ifndef NO_HASHDB
  if (dirty && ISFLAG(flags, F_HASHDB)) add_hashdb_entry(NULL, 0, file);
#endif
  return 0;
}

static int pipeline_confirm(file_t *file, file_t *match)
{
  int cmpresult;

  cmpresult = check_conditions(match, file);
  if (cmpresult == 2) return 1;
  if (cmpresult != 0) return 0;

  if (
        ISFLAG(flags, F_QUICKCOMPARE)
     || ISFLAG(flags, F_PARTIALONLY)
#ifndef NO_HARDLINKS
     || (ISFLAG(flags, F_CONSIDERHARDLINKS)
     &&  (file->inode == match->inode)
     &&  (file->device == match->device))
#endif
     ) return 1;

#ifdef HDUPES_HAS_THREADS
  return confirmmatch_parallel(file->d_name, match->d_name, file->size, hash_threads) == 0;
#else
  return confirmmatch(file->d_name, match->d_name, file->size) == 0;
#endif
}

static void pipeline_register(file_t **head, file_t *file, ordertype_t ordertype)
{
#ifndef NO_MTIME
  registerpair(head, file, (ordertype == ORDER_TIME) ? sort_pairs_by_mtime : sort_pairs_by_filename);
#else
  (void)ordertype;
  registerpair(head, file, sort_pairs_by_filename);
#endif
  dupecount++;
}

static int pipeline_process_hash_group(pipeline_entry_t *entries, size_t count, ordertype_t ordertype)
{
  file_t **heads = NULL;
  size_t head_count = 0;
  size_t i;

  heads = (file_t **)calloc(count, sizeof(file_t *));
  if (heads == NULL) return -1;

  for (i = 0; i < count; i++) {
    file_t *file = entries[i].file;
    size_t h;
    int matched = 0;

    if (interrupt) {
      free(heads);
      return -1;
    }

    for (h = 0; h < head_count; h++) {
      if (pipeline_confirm(file, heads[h])) {
        pipeline_register(&heads[h], file, ordertype);
        matched = 1;
        break;
      }
    }

    if (!matched) heads[head_count++] = file;
  }

  free(heads);
  return 0;
}

int pipeline_match_files(file_t *files, ordertype_t ordertype)
{
  pipeline_entry_t *entries = NULL;
  size_t count = 0;
  size_t i = 0;
  file_t *cur = files;

  while (cur != NULL) {
    count++;
    cur = cur->next;
  }
  if (count == 0) return 0;

  entries = (pipeline_entry_t *)calloc(count, sizeof(pipeline_entry_t));
  if (entries == NULL) return -1;

  cur = files;
  while (cur != NULL) {
    entries[i].file = cur;
    entries[i].index = (uintmax_t)i;
    i++;
    cur = cur->next;
  }

  qsort(entries, count, sizeof(pipeline_entry_t), cmp_entry_size);

  i = 0;
  while (i < count) {
    size_t size_start = i;
    off_t size = entries[i].file->size;

    while (i < count && entries[i].file->size == size) i++;
    if (i - size_start < 2) continue;

    {
      size_t j;
      for (j = size_start; j < i; j++) {
        if (ensure_pipeline_hashes(entries[j].file) != 0) {
          free(entries);
          return -1;
        }
      }
    }

    qsort(entries + size_start, i - size_start, sizeof(pipeline_entry_t), cmp_entry_full_hash);

    {
      size_t j = size_start;
      while (j < i) {
        size_t hash_start = j;
        uint64_t hash = entries[j].file->filehash;

        while (j < i && entries[j].file->filehash == hash) j++;
        if (j - hash_start > 1 &&
            pipeline_process_hash_group(entries + hash_start, j - hash_start, ordertype) != 0) {
          free(entries);
          return -1;
        }
      }
    }
  }

  free(entries);
  return 0;
}
