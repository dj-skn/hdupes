/* hdupes directory scanning code
 * This file is part of hdupes; see hdupes.c for license information */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
 #include <sys/attr.h>
#endif

#include <libjodycode.h>
#include "likely_unlikely.h"
#include "hdupes.h"
#include "checks.h"
#ifndef NO_EXTFILTER
 #include "extfilter.h"
#endif
#include "filestat.h"
#ifndef NO_HASHDB
 #include "hashdb.h"
#endif
#include "progress.h"
#include "interrupt.h"
#ifndef NO_TRAVCHECK
 #include "travcheck.h"
#endif

/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __MINGW32__
 const char dir_sep = '\\';
#else /* Not Windows */
 const char dir_sep = '/';
#endif /* _WIN32 || __MINGW32__ */

void loaddir(char * const restrict dir,
                file_t * restrict * const restrict filelistp,
                int recurse);

#ifdef HDUPES_HAS_THREADS
#include <pthread.h>

#define PARALLEL_SEEN_BUCKETS 8192
#define DARWIN_BULK_BUFSIZE 65536

typedef struct dir_task {
  char *path;
  int recurse;
  unsigned int user_order;
  dev_t parent_device;
  struct dir_task *next;
} dir_task_t;

typedef struct seen_dir {
  dev_t device;
  hdupes_ino_t inode;
  struct seen_dir *next;
} seen_dir_t;

typedef struct parallel_walk_ctx {
  dir_task_t *head;
  dir_task_t *tail;
  unsigned int active;
  int done;
  int error;
  pthread_mutex_t queue_lock;
  pthread_cond_t queue_cond;

  file_t **files;
  size_t file_count;
  size_t file_cap;
  pthread_mutex_t files_lock;

  seen_dir_t *seen[PARALLEL_SEEN_BUCKETS];
  pthread_mutex_t seen_lock;

  pthread_mutex_t warning_lock;
  int single_file_warning;
} parallel_walk_ctx_t;

static char *xstrdup_local(const char *src)
{
  char *dst;
  size_t len;

  if (src == NULL) jc_nullptr("xstrdup_local()");
  len = strlen(src) + 1;
  dst = (char *)malloc(len);
  if (dst == NULL) jc_oom("xstrdup_local()");
  memcpy(dst, src, len);
  return dst;
}

static file_t *parallel_newfile(const char *path, const unsigned int order)
{
  file_t *newfile;
  size_t len;

  if (path == NULL) jc_nullptr("parallel_newfile()");
  len = strlen(path) + 1;
  newfile = (file_t *)calloc(1, sizeof(file_t));
  if (newfile == NULL) jc_oom("parallel_newfile()");
  newfile->d_name = (char *)malloc(EXTEND64(len + 1));
  if (newfile->d_name == NULL) jc_oom("parallel_newfile() filename");
  memcpy(newfile->d_name, path, len);
  newfile->next = NULL;
  newfile->duplicates = NULL;
  newfile->size = -1;
#ifndef NO_USER_ORDER
  newfile->user_order = order;
#endif
  return newfile;
}

static void parallel_free_file(file_t *file)
{
  if (file == NULL) return;
  free(file->d_name);
  free(file);
}

static int parallel_is_hidden_path(const char *path)
{
  const char *base;

  if (path == NULL) jc_nullptr("parallel_is_hidden_path()");
  base = strrchr(path, '/');
  if (base == NULL) base = strrchr(path, '\\');
  base = (base == NULL) ? path : base + 1;
  return base[0] == '.' && jc_streq(base, ".") != 0 && jc_streq(base, "..") != 0;
}

static int parallel_check_singlefile(file_t *newfile)
{
  if (newfile == NULL) jc_nullptr("parallel_check_singlefile()");

  if (ISFLAG(flags, F_EXCLUDEHIDDEN) && parallel_is_hidden_path(newfile->d_name))
    return 1;

  if (getfilestats(newfile) != 0 || newfile->size == -1) return 1;
  if (!JC_S_ISREG(newfile->mode) && !JC_S_ISDIR(newfile->mode)) return 1;

  if (!JC_S_ISDIR(newfile->mode)) {
    if (newfile->size == 0 && !ISFLAG(flags, F_INCLUDEEMPTY)) return 1;
#ifndef NO_EXTFILTER
    if (extfilter_exclude(newfile)) return 1;
#endif
  }

#ifdef ON_WINDOWS
#ifndef NO_HARDLINKS
  if (ISFLAG(a_flags, FA_HARDLINKFILES) && newfile->nlink >= 1024) return 1;
#endif
#endif

  return 0;
}

static uintmax_t seen_hash(const dev_t device, const hdupes_ino_t inode)
{
  return (((uintmax_t)device << 16) ^ (uintmax_t)inode) % PARALLEL_SEEN_BUCKETS;
}

static int seen_check_add(parallel_walk_ctx_t *ctx, const dev_t device, const hdupes_ino_t inode)
{
  uintmax_t bucket;
  seen_dir_t *cur;
  seen_dir_t *node;

  if (ctx == NULL) jc_nullptr("seen_check_add()");
  if (ISFLAG(flags, F_NOTRAVCHECK)) return 0;

  bucket = seen_hash(device, inode);
  pthread_mutex_lock(&ctx->seen_lock);
  cur = ctx->seen[bucket];
  while (cur != NULL) {
    if (cur->device == device && cur->inode == inode) {
      pthread_mutex_unlock(&ctx->seen_lock);
      return 1;
    }
    cur = cur->next;
  }

  node = (seen_dir_t *)malloc(sizeof(seen_dir_t));
  if (node == NULL) jc_oom("seen_check_add()");
  node->device = device;
  node->inode = inode;
  node->next = ctx->seen[bucket];
  ctx->seen[bucket] = node;
  pthread_mutex_unlock(&ctx->seen_lock);
  return 0;
}

static void queue_push(parallel_walk_ctx_t *ctx, char *path, const int recurse,
                       const unsigned int order, const dev_t parent_device)
{
  dir_task_t *task;

  if (ctx == NULL || path == NULL) jc_nullptr("queue_push()");
  task = (dir_task_t *)malloc(sizeof(dir_task_t));
  if (task == NULL) jc_oom("queue_push()");
  task->path = path;
  task->recurse = recurse;
  task->user_order = order;
  task->parent_device = parent_device;
  task->next = NULL;

  pthread_mutex_lock(&ctx->queue_lock);
  if (ctx->tail != NULL) ctx->tail->next = task;
  else ctx->head = task;
  ctx->tail = task;
  pthread_cond_signal(&ctx->queue_cond);
  pthread_mutex_unlock(&ctx->queue_lock);
}

static dir_task_t *queue_pop(parallel_walk_ctx_t *ctx)
{
  dir_task_t *task;

  if (ctx == NULL) jc_nullptr("queue_pop()");
  pthread_mutex_lock(&ctx->queue_lock);
  while (ctx->head == NULL && ctx->active > 0 && !ctx->done)
    pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);

  if (ctx->head == NULL) {
    ctx->done = 1;
    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);
    return NULL;
  }

  task = ctx->head;
  ctx->head = task->next;
  if (ctx->head == NULL) ctx->tail = NULL;
  ctx->active++;
  pthread_mutex_unlock(&ctx->queue_lock);
  task->next = NULL;
  return task;
}

static void queue_task_done(parallel_walk_ctx_t *ctx)
{
  if (ctx == NULL) jc_nullptr("queue_task_done()");
  pthread_mutex_lock(&ctx->queue_lock);
  if (ctx->active > 0) ctx->active--;
  if (ctx->head == NULL && ctx->active == 0) {
    ctx->done = 1;
    pthread_cond_broadcast(&ctx->queue_cond);
  } else {
    pthread_cond_signal(&ctx->queue_cond);
  }
  pthread_mutex_unlock(&ctx->queue_lock);
}

static void parallel_add_file(parallel_walk_ctx_t *ctx, file_t *file)
{
  if (ctx == NULL || file == NULL) jc_nullptr("parallel_add_file()");

  pthread_mutex_lock(&ctx->files_lock);
  if (ctx->file_count == ctx->file_cap) {
    size_t newcap = ctx->file_cap == 0 ? 1024 : ctx->file_cap * 2;
    file_t **newfiles = (file_t **)realloc(ctx->files, sizeof(file_t *) * newcap);
    if (newfiles == NULL) jc_oom("parallel_add_file()");
    ctx->files = newfiles;
    ctx->file_cap = newcap;
  }
  ctx->files[ctx->file_count++] = file;
  pthread_mutex_unlock(&ctx->files_lock);
}

static void warn_single_file(parallel_walk_ctx_t *ctx)
{
  if (ctx == NULL) jc_nullptr("warn_single_file()");
  pthread_mutex_lock(&ctx->warning_lock);
  if (ctx->single_file_warning == 0) {
    fprintf(stderr, "\nFile specs on command line disabled in this version for safety\n");
    fprintf(stderr, "This should be restored (and safe) in a future release\n");
    fprintf(stderr, "More info at hdupes.com or email jody@jodybruchon.com\n");
    ctx->single_file_warning = 1;
  }
  pthread_mutex_unlock(&ctx->warning_lock);
}

static int process_child_path(parallel_walk_ctx_t *ctx, const char *dir,
                              const size_t dirlen, const char *name,
                              const int recurse, const unsigned int order,
                              const dev_t device)
{
  char pathbuf[PATHBUF_SIZE * 2];
  size_t namelen;
  size_t pos;
  file_t *newfile;

  if (ctx == NULL || dir == NULL || name == NULL) jc_nullptr("process_child_path()");
  if (!jc_streq(name, ".") || !jc_streq(name, "..")) return 0;

  namelen = strlen(name);
  pos = dirlen;
  if (pos + namelen + 2 >= sizeof(pathbuf)) {
    fprintf(stderr, "\nerror: a path overflowed (longer than PATHBUF_SIZE) cannot continue\n");
    ctx->error = 1;
    return -1;
  }

  memcpy(pathbuf, dir, pos + 1);
  if (pos != 0 && pathbuf[pos - 1] != dir_sep) pathbuf[pos++] = dir_sep;
  memcpy(pathbuf + pos, name, namelen);
  pathbuf[pos + namelen] = '\0';

  newfile = parallel_newfile(pathbuf, order);
  if (parallel_check_singlefile(newfile) != 0) {
    parallel_free_file(newfile);
    return 0;
  }

  if (JC_S_ISDIR(newfile->mode)) {
    if (recurse) {
      if (ISFLAG(flags, F_ONEFS) && device != newfile->device) {
        parallel_free_file(newfile);
        return 0;
      }
#ifndef NO_SYMLINKS
      if (ISFLAG(flags, F_FOLLOWLINKS) || !ISFLAG(newfile->flags, FF_IS_SYMLINK)) {
        char *task_path = newfile->d_name;
        free(newfile);
        queue_push(ctx, task_path, recurse, order, device);
      } else {
        parallel_free_file(newfile);
      }
#else
      {
        char *task_path = newfile->d_name;
        free(newfile);
        queue_push(ctx, task_path, recurse, order, device);
      }
#endif
    } else {
      parallel_free_file(newfile);
    }
    return 0;
  }

#ifndef NO_SYMLINKS
  if (!ISFLAG(newfile->flags, FF_IS_SYMLINK) ||
      (ISFLAG(newfile->flags, FF_IS_SYMLINK) && ISFLAG(flags, F_FOLLOWLINKS))) {
#else
  if (JC_S_ISREG(newfile->mode)) {
#endif
    parallel_add_file(ctx, newfile);
  } else {
    parallel_free_file(newfile);
  }

  return 0;
}

#ifdef __APPLE__
static int scan_dir_darwin_bulk(parallel_walk_ctx_t *ctx, const char *dir,
                                const int recurse, const unsigned int order,
                                const dev_t device)
{
  int fd;
  int count;
  struct attrlist attrs;
  char buf[DARWIN_BULK_BUFSIZE];
  size_t dirlen;

  memset(&attrs, 0, sizeof(attrs));
  attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
  attrs.commonattr = ATTR_CMN_NAME;

  fd = open(dir, O_RDONLY);
  if (fd < 0) return -1;
  dirlen = strlen(dir);

  for (;;) {
    char *cursor = buf;
    int i;

    count = getattrlistbulk(fd, &attrs, buf, sizeof(buf), 0);
    if (count < 0) {
      close(fd);
      return -1;
    }
    if (count == 0) break;

    for (i = 0; i < count; i++) {
      uint32_t length;
      attrreference_t name_ref;
      char *name;

      if ((size_t)(cursor - buf) + sizeof(uint32_t) + sizeof(attrreference_t) > sizeof(buf)) {
        close(fd);
        return -1;
      }
      memcpy(&length, cursor, sizeof(length));
      if (length < sizeof(uint32_t) + sizeof(attrreference_t) ||
          (size_t)(cursor - buf) + length > sizeof(buf)) {
        close(fd);
        return -1;
      }
      memcpy(&name_ref, cursor + sizeof(uint32_t), sizeof(name_ref));
      if (name_ref.attr_dataoffset < 0 ||
          (uint32_t)name_ref.attr_dataoffset >= length ||
          (uint32_t)name_ref.attr_dataoffset + name_ref.attr_length > length) {
        close(fd);
        return -1;
      }
      name = cursor + name_ref.attr_dataoffset;
      if (process_child_path(ctx, dir, dirlen, name, recurse, order, device) != 0) {
        close(fd);
        return -1;
      }
      cursor += length;
    }
  }

  close(fd);
  return 0;
}
#endif /* __APPLE__ */

static int scan_dir_readdir(parallel_walk_ctx_t *ctx, const char *dir,
                            const int recurse, const unsigned int order,
                            const dev_t device)
{
  JC_DIR *cd;
  struct JC_DIRENT *dirinfo;
  size_t dirlen;

  cd = jc_opendir(dir);
  if (cd == NULL) return -1;
  dirlen = strlen(dir);

  while ((dirinfo = jc_readdir(cd)) != NULL) {
    if (interrupt) {
      jc_closedir(cd);
      return -1;
    }
    if (process_child_path(ctx, dir, dirlen, dirinfo->d_name, recurse, order, device) != 0) {
      jc_closedir(cd);
      return -1;
    }
  }

  jc_closedir(cd);
  return 0;
}

static void process_dir_task(parallel_walk_ctx_t *ctx, dir_task_t *task)
{
  hdupes_ino_t inode;
  dev_t device;
  hdupes_mode_t mode;
  int type;

  if (ctx == NULL || task == NULL) jc_nullptr("process_dir_task()");
  if (interrupt) return;

  jc_slash_convert(task->path);
  type = getdirstats(task->path, &inode, &device, &mode);
  if (type < 0) {
    fprintf(stderr, "\ncould not stat dir "); jc_fwprint(stderr, task->path, 1);
    exit_status = EXIT_FAILURE;
    return;
  }
  if (type == 1) {
    warn_single_file(ctx);
    return;
  }

  if (seen_check_add(ctx, device, inode) != 0) return;
  __sync_fetch_and_add(&item_progress, 1);

#ifdef __APPLE__
  if (scan_dir_darwin_bulk(ctx, task->path, task->recurse, task->user_order, device) == 0)
    return;
#endif

  if (scan_dir_readdir(ctx, task->path, task->recurse, task->user_order, device) != 0) {
    fprintf(stderr, "\ncould not chdir to "); jc_fwprint(stderr, task->path, 1);
    exit_status = EXIT_FAILURE;
  }
}

static void *parallel_walk_worker(void *arg)
{
  parallel_walk_ctx_t *ctx = (parallel_walk_ctx_t *)arg;

  for (;;) {
    dir_task_t *task = queue_pop(ctx);
    if (task == NULL) break;
    process_dir_task(ctx, task);
    free(task->path);
    free(task);
    queue_task_done(ctx);
  }

  return NULL;
}

static int cmp_file_ptr_path(const void *a, const void *b)
{
  const file_t *fa = *(file_t * const *)a;
  const file_t *fb = *(file_t * const *)b;

#ifndef NO_USER_ORDER
  if (fa->user_order < fb->user_order) return -1;
  if (fa->user_order > fb->user_order) return 1;
#endif
  return strcmp(fa->d_name, fb->d_name);
}

static void append_parallel_files(file_t * restrict * const restrict filelistp,
                                  file_t **files, const size_t count)
{
  file_t *tail;
  size_t i;

  if (filelistp == NULL) jc_nullptr("append_parallel_files()");
  if (count == 0) return;

  qsort(files, count, sizeof(file_t *), cmp_file_ptr_path);

  if (*filelistp == NULL) {
    *filelistp = files[0];
  } else {
    tail = *filelistp;
    while (tail->next != NULL) tail = tail->next;
    tail->next = files[0];
  }

  for (i = 0; i < count - 1; i++) files[i]->next = files[i + 1];
  files[count - 1]->next = NULL;

  for (i = 0; i < count; i++) {
#ifndef NO_HASHDB
    if (ISFLAG(flags, F_HASHDB)) read_hashdb_entry(files[i]);
#endif
    filecount++;
    progress++;
  }
}

static void free_seen_dirs(parallel_walk_ctx_t *ctx)
{
  size_t i;

  if (ctx == NULL) return;
  for (i = 0; i < PARALLEL_SEEN_BUCKETS; i++) {
    seen_dir_t *cur = ctx->seen[i];
    while (cur != NULL) {
      seen_dir_t *next = cur->next;
      free(cur);
      cur = next;
    }
  }
}

void loaddir_parallel_batch(char **dirs, const int first, const int last,
                            file_t * restrict * const restrict filelistp,
                            const int recurse, const unsigned int threads)
{
  parallel_walk_ctx_t ctx;
  pthread_t *tids;
  unsigned int i;
  int x;
  unsigned int start_order;

  if (dirs == NULL || filelistp == NULL) jc_nullptr("loaddir_parallel_batch()");
  if (first >= last) return;
  if (threads <= 1) {
    for (x = first; x < last; x++) {
      loaddir(dirs[x], filelistp, recurse);
      user_item_count++;
    }
    return;
  }

  memset(&ctx, 0, sizeof(ctx));
  pthread_mutex_init(&ctx.queue_lock, NULL);
  pthread_cond_init(&ctx.queue_cond, NULL);
  pthread_mutex_init(&ctx.files_lock, NULL);
  pthread_mutex_init(&ctx.seen_lock, NULL);
  pthread_mutex_init(&ctx.warning_lock, NULL);

  start_order = user_item_count;
  for (x = first; x < last; x++)
    queue_push(&ctx, xstrdup_local(dirs[x]), recurse, start_order + (unsigned int)(x - first), 0);

  tids = (pthread_t *)calloc(threads, sizeof(pthread_t));
  if (tids == NULL) jc_oom("loaddir_parallel_batch()");

  for (i = 0; i < threads; i++) pthread_create(&tids[i], NULL, parallel_walk_worker, &ctx);
  for (i = 0; i < threads; i++) pthread_join(tids[i], NULL);

  append_parallel_files(filelistp, ctx.files, ctx.file_count);
  user_item_count += (unsigned int)(last - first);

  free(tids);
  free(ctx.files);
  free_seen_dirs(&ctx);
  pthread_mutex_destroy(&ctx.queue_lock);
  pthread_cond_destroy(&ctx.queue_cond);
  pthread_mutex_destroy(&ctx.files_lock);
  pthread_mutex_destroy(&ctx.seen_lock);
  pthread_mutex_destroy(&ctx.warning_lock);
}
#endif /* HDUPES_HAS_THREADS */

static file_t *init_newfile(const size_t len, file_t * restrict * const restrict filelistp)
{
  file_t * const restrict newfile = (file_t *)calloc(1, sizeof(file_t));

  if (unlikely(!newfile)) jc_oom("init_newfile() file structure");
  if (unlikely(!filelistp)) jc_nullptr("init_newfile() filelistp");

  LOUD(fprintf(stderr, "init_newfile(len %" PRIuMAX ", filelistp %p)\n", (uintmax_t)len, filelistp));

  newfile->d_name = (char *)malloc(EXTEND64(len));
  if (!newfile->d_name) jc_oom("init_newfile() filename");

  newfile->next = *filelistp;
#ifndef NO_USER_ORDER
  newfile->user_order = user_item_count;
#endif
  newfile->size = -1;
  newfile->duplicates = NULL;
  return newfile;
}


/* This is disabled until a check is in place to make it safe */
#if 0
/* Add a single file to the file tree */
file_t *grokfile(const char * const restrict name, file_t * restrict * const restrict filelistp)
{
  file_t * restrict newfile;

  if (!name || !filelistp) jc_nullptr("grokfile()");
  LOUD(fprintf(stderr, "grokfile: '%s' %p\n", name, filelistp));

  /* Allocate the file_t and the d_name entries */
  newfile = init_newfile(strlen(name) + 2, filelistp);

  strcpy(newfile->d_name, name);

  /* Single-file [l]stat() and exclusion condition check */
  if (check_singlefile(newfile) != 0) {
    LOUD(fprintf(stderr, "grokfile: check_singlefile rejected file\n"));
    free(newfile->d_name);
    free(newfile);
    return NULL;
  }
  return newfile;
}
#endif

/* Load a directory's contents into the file tree, recursing as needed */
void loaddir(char * const restrict dir,
                file_t * restrict * const restrict filelistp,
                int recurse)
{
  file_t * restrict newfile;
  struct JC_DIRENT *dirinfo;
  size_t dirlen, dirpos;
  int i;
//  single = 0;
  hdupes_ino_t inode;
  dev_t device, n_device;
  hdupes_mode_t mode;
  JC_DIR *cd;
  static int sf_warning = 0; /* single file warning should only appear once */

  if (unlikely(dir == NULL || filelistp == NULL)) jc_nullptr("loaddir()");
  LOUD(fprintf(stderr, "loaddir: scanning '%s' (order %d, recurse %d)\n", dir, user_item_count, recurse));

  if (unlikely(interrupt != 0)) return;

  /* Convert forward slashes to backslashes if on Windows */
  jc_slash_convert(dir);

  /* Get directory stats (or file stats if it's a file) */
  i = getdirstats(dir, &inode, &device, &mode);
  if (unlikely(i < 0)) goto error_stat_dir;

  /* if dir is actually a file, just add it to the file tree */
  if (i == 1) {
/* Single file addition is disabled for now because there is no safeguard
 * against the file being compared against itself if it's added in both a
 * recursion and explicitly on the command line. */
#if 0
    LOUD(fprintf(stderr, "loaddir -> grokfile '%s'\n", dir));
    newfile = grokfile(dir, filelistp);
    if (newfile == NULL) {
      LOUD(fprintf(stderr, "grokfile rejected '%s'\n", dir));
      return;
    }
    single = 1;
    goto add_single_file;
#endif
    if (sf_warning == 0) {
      fprintf(stderr, "\nFile specs on command line disabled in this version for safety\n");
      fprintf(stderr, "This should be restored (and safe) in a future release\n");
      fprintf(stderr, "More info at hdupes.com or email jody@jodybruchon.com\n");
      sf_warning = 1;
    }
    return; /* Remove when single file is restored */
  }

/* Double traversal prevention tree */
#ifndef NO_TRAVCHECK
  if (likely(!ISFLAG(flags, F_NOTRAVCHECK))) {
    i = traverse_check(device, inode);
    if (unlikely(i == 1)) return;
    if (unlikely(i == 2)) goto error_stat_dir;
  }
#endif /* NO_TRAVCHECK */

  item_progress++;

  cd = jc_opendir(dir);
  if (unlikely(!cd)) goto error_cd;
  dirlen = strlen(dir);

  while ((dirinfo = jc_readdir(cd)) != NULL) {
    char * restrict tp = tempname;
    size_t d_name_len;

    if (unlikely(interrupt != 0)) return;
    LOUD(fprintf(stderr, "loaddir: readdir: '%s'\n", dirinfo->d_name));
    if (unlikely(!jc_streq(dirinfo->d_name, ".") || !jc_streq(dirinfo->d_name, ".."))) continue;
    check_sigusr1();
    if (jc_alarm_ring != 0) {
      jc_alarm_ring = 0;
      update_phase1_progress("dirs");
    }

    /* Assemble the file's full path name, optimized to avoid strcat() */
    dirpos = dirlen;
    d_name_len = strlen(dirinfo->d_name);
    memcpy(tp, dir, dirpos + 1);
    if (dirpos != 0 && tp[dirpos - 1] != dir_sep) {
      tp[dirpos] = dir_sep;
      dirpos++;
    }
    if (unlikely(dirpos + d_name_len + 1 >= (PATHBUF_SIZE * 2))) goto error_overflow;
    tp += dirpos;
    memcpy(tp, dirinfo->d_name, d_name_len);
    tp += d_name_len;
    *tp = '\0';
    d_name_len++;

    /* Allocate the file_t and the d_name entries */
    newfile = init_newfile(dirpos + d_name_len + 2, filelistp);

    tp = tempname;
    memcpy(newfile->d_name, tp, dirpos + d_name_len);

    /*** WARNING: tempname global gets reused by check_singlefile here! ***/

    /* Single-file [l]stat() and exclusion condition check */
    if (check_singlefile(newfile) != 0) {
      LOUD(fprintf(stderr, "loaddir: check_singlefile rejected file\n"));
      free(newfile->d_name);
      free(newfile);
      continue;
    }

    /* Optionally recurse directories, including symlinked ones if requested */
    if (JC_S_ISDIR(newfile->mode)) {
      if (recurse) {
        /* --one-file-system - WARNING: this clobbers inode/mode */
        if (ISFLAG(flags, F_ONEFS)
            && (getdirstats(newfile->d_name, &inode, &n_device, &mode) == 0)
            && (device != n_device)) {
          LOUD(fprintf(stderr, "loaddir: directory: not recursing (--one-file-system)\n"));
          free(newfile->d_name);
          free(newfile);
          continue;
        }
#ifndef NO_SYMLINKS
        else if (ISFLAG(flags, F_FOLLOWLINKS) || !ISFLAG(newfile->flags, FF_IS_SYMLINK)) {
          LOUD(fprintf(stderr, "loaddir: directory(symlink): recursing (-r/-R)\n"));
          loaddir(newfile->d_name, filelistp, recurse);
        }
#else
        else {
          LOUD(fprintf(stderr, "loaddir: directory: recursing (-r/-R)\n"));
          loaddir(newfile->d_name, filelistp, recurse);
        }
#endif /* NO_SYMLINKS */
      } else { LOUD(fprintf(stderr, "loaddir: directory: not recursing\n")); }
      free(newfile->d_name);
      free(newfile);
      if (unlikely(interrupt != 0)) return;
      continue;
    } else {
//add_single_file:
      /* Add regular files to list, including symlink targets if requested */
#ifndef NO_SYMLINKS
      if (!ISFLAG(newfile->flags, FF_IS_SYMLINK) || (ISFLAG(newfile->flags, FF_IS_SYMLINK) && ISFLAG(flags, F_FOLLOWLINKS))) {
#else
      if (JC_S_ISREG(newfile->mode)) {
#endif
#ifndef NO_HASHDB
        if (ISFLAG(flags, F_HASHDB)) read_hashdb_entry(newfile);
#endif
        *filelistp = newfile;
        filecount++;
        progress++;

      } else {
        LOUD(fprintf(stderr, "loaddir: not a regular file: %s\n", newfile->d_name);)
        free(newfile->d_name);
        free(newfile);
//    if (single == 1) return;
        continue;
      }
    }
    /* Skip directory stuff if adding only a single file */
//    if (single == 1) return;
  }

  jc_closedir(cd);

  return;

error_stat_dir:
  fprintf(stderr, "\ncould not stat dir "); jc_fwprint(stderr, dir, 1);
  exit_status = EXIT_FAILURE;
  return;
error_cd:
  fprintf(stderr, "\ncould not chdir to "); jc_fwprint(stderr, dir, 1);
  exit_status = EXIT_FAILURE;
  return;
error_overflow:
  fprintf(stderr, "\nerror: a path overflowed (longer than PATHBUF_SIZE) cannot continue\n");
  exit(EXIT_FAILURE);
}
