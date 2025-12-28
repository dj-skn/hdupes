/* Parallel hashing support
 * This file is part of hdupes; see hdupes.c for license information */

#include "hashpool.h"

#ifdef HDUPES_HAS_THREADS
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "filehash.h"
#include "interrupt.h"
#ifndef NO_HASHDB
#include "hashdb.h"
#endif

typedef struct hashpool_ctx {
  file_t **items;
  size_t count;
  volatile size_t index;
  int do_full;
  int use_hashdb;
#ifndef NO_HASHDB
  pthread_mutex_t hashdb_lock;
#endif
} hashpool_ctx_t;

static int compute_partial_hash(file_t *file, void *buffer)
{
  uint64_t hash = 0;

  if (ISFLAG(file->flags, FF_HASH_PARTIAL)) return 0;
  if (compute_filehash_with_buffer(file, PARTIAL_HASH_SIZE, hash_algo, &hash, buffer, auto_chunk_size, 0) != 0)
    return -1;

  file->filehash_partial = hash;
  SETFLAG(file->flags, FF_HASH_PARTIAL);
  return 0;
}

static int compute_full_hash(file_t *file, void *buffer)
{
  uint64_t hash = 0;

  if (ISFLAG(file->flags, FF_HASH_FULL)) return 0;
  if (compute_filehash_with_buffer(file, 0, hash_algo, &hash, buffer, auto_chunk_size, 0) != 0)
    return -1;

  file->filehash = hash;
  SETFLAG(file->flags, FF_HASH_FULL);
  return 0;
}

static void maybe_add_hashdb(hashpool_ctx_t *ctx, const file_t *file)
{
#ifndef NO_HASHDB
  if (!ctx->use_hashdb) return;
  pthread_mutex_lock(&ctx->hashdb_lock);
  add_hashdb_entry(NULL, 0, file);
  pthread_mutex_unlock(&ctx->hashdb_lock);
#else
  (void)ctx;
  (void)file;
#endif
}

static void *hash_worker(void *arg)
{
  hashpool_ctx_t *ctx = (hashpool_ctx_t *)arg;
  void *buffer;

  buffer = malloc(auto_chunk_size);
  if (buffer == NULL) return NULL;

  for (;;) {
    size_t idx = __sync_fetch_and_add(&ctx->index, 1);
    file_t *file;

    if (idx >= ctx->count) break;
    if (interrupt) break;

    file = ctx->items[idx];
    if (file == NULL) continue;

    if (compute_partial_hash(file, buffer) != 0) continue;

    if (file->size <= PARTIAL_HASH_SIZE || ISFLAG(flags, F_PARTIALONLY)) {
      if (!ISFLAG(file->flags, FF_HASH_FULL)) {
        file->filehash = file->filehash_partial;
        SETFLAG(file->flags, FF_HASH_FULL);
      }
      maybe_add_hashdb(ctx, file);
      continue;
    }

    if (ctx->do_full) {
      if (compute_full_hash(file, buffer) == 0)
        maybe_add_hashdb(ctx, file);
    }
  }

  free(buffer);
  return NULL;
}

int prehash_files(file_t *files, unsigned int threads)
{
  size_t count = 0;
  size_t i = 0;
  file_t *cur = files;
  file_t **items = NULL;
  pthread_t *tids = NULL;
  hashpool_ctx_t ctx;

  if (files == NULL || threads <= 1) return 0;

  while (cur) {
    count++;
    cur = cur->next;
  }
  if (count == 0) return 0;

  items = (file_t **)calloc(count, sizeof(file_t *));
  if (items == NULL) return -1;

  cur = files;
  while (cur) {
    items[i++] = cur;
    cur = cur->next;
  }

  ctx.items = items;
  ctx.count = count;
  ctx.index = 0;
  ctx.do_full = !ISFLAG(flags, F_PARTIALONLY);
  ctx.use_hashdb = ISFLAG(flags, F_HASHDB);
#ifndef NO_HASHDB
  if (ctx.use_hashdb) pthread_mutex_init(&ctx.hashdb_lock, NULL);
#endif

  tids = (pthread_t *)calloc(threads, sizeof(pthread_t));
  if (tids == NULL) {
    free(items);
#ifndef NO_HASHDB
    if (ctx.use_hashdb) pthread_mutex_destroy(&ctx.hashdb_lock);
#endif
    return -1;
  }

  for (i = 0; i < threads; i++) pthread_create(&tids[i], NULL, hash_worker, &ctx);
  for (i = 0; i < threads; i++) pthread_join(tids[i], NULL);

#ifndef NO_HASHDB
  if (ctx.use_hashdb) pthread_mutex_destroy(&ctx.hashdb_lock);
#endif
  free(tids);
  free(items);
  return 0;
}

#else  /* HDUPES_HAS_THREADS */

int prehash_files(file_t *files, unsigned int threads)
{
  (void)files;
  (void)threads;
  return 0;
}

#endif /* HDUPES_HAS_THREADS */
