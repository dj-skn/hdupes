/* Print matched file sets
 * This file is part of hdupes; see hdupes.c for license information */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "hdupes.h"
#include <libjodycode.h>
#include "act_printmatches.h"

typedef struct {
  file_t *head;
  uintmax_t count;
  uintmax_t total_bytes;
} group_info_t;

static void format_bytes(uintmax_t bytes, char *out, size_t out_size)
{
  if (bytes < 1000) {
    snprintf(out, out_size, "%" PRIuMAX "B", bytes);
  } else if (bytes < 1000 * 1000) {
    snprintf(out, out_size, "%.1fKB", (double)bytes / 1000.0);
  } else if (bytes < 1000ULL * 1000ULL * 1000ULL) {
    snprintf(out, out_size, "%.1fMB", (double)bytes / 1000000.0);
  } else {
    snprintf(out, out_size, "%.1fGB", (double)bytes / 1000000000.0);
  }
}

static int group_sort_cmp(const void *a, const void *b)
{
  const group_info_t *ga = (const group_info_t *)a;
  const group_info_t *gb = (const group_info_t *)b;

  if (ga->total_bytes > gb->total_bytes) return -1;
  if (ga->total_bytes < gb->total_bytes) return 1;
  if (ga->count > gb->count) return -1;
  if (ga->count < gb->count) return 1;
  return strcmp(ga->head->d_name, gb->head->d_name);
}

static void print_group(file_t *head, int cr)
{
  file_t *tmpfile;

  if (!ISFLAG(a_flags, FA_OMITFIRST)) {
    if (ISFLAG(a_flags, FA_SHOWSIZE))
      printf("%" PRIdMAX " byte%c each:\n", (intmax_t)head->size, (head->size != 1) ? 's' : ' ');
    jc_fwprint(stdout, head->d_name, cr);
  }
  tmpfile = head->duplicates;
  while (tmpfile != NULL) {
    jc_fwprint(stdout, tmpfile->d_name, cr);
    tmpfile = tmpfile->duplicates;
  }
}

static void print_group_pretty(const group_info_t *group, int group_index)
{
  char size_buf[32];
  file_t *tmpfile = NULL;

  format_bytes((uintmax_t)group->head->size, size_buf, sizeof(size_buf));
  printf("➤ Group %d · %" PRIuMAX " files · %s each\n", group_index, group->count, size_buf);

  if (!ISFLAG(a_flags, FA_OMITFIRST)) {
    printf("  → %s\n", group->head->d_name);
  }
  tmpfile = group->head->duplicates;
  while (tmpfile != NULL) {
    printf("  → %s\n", tmpfile->d_name);
    tmpfile = tmpfile->duplicates;
  }
}

void printmatches(file_t * restrict files)
{
  file_t * restrict tmpfile;
  int printed = 0;
  int cr = 1;
  int pretty = 1;

  LOUD(fprintf(stderr, "printmatches: %p\n", files));

  if (ISFLAG(a_flags, FA_PRINTNULL)) {
    cr = 2;
    pretty = 0;
  }

  if (ISFLAG(flags, F_SORTGROUPS)) {
    size_t group_count = 0;
    size_t idx = 0;
    file_t *scan = files;
    group_info_t *groups = NULL;
    uintmax_t total_bytes = 0;
    uintmax_t total_files = 0;

    while (scan != NULL) {
      if (ISFLAG(scan->flags, FF_HAS_DUPES)) group_count++;
      scan = scan->next;
    }

    if (group_count > 0) groups = (group_info_t *)calloc(group_count, sizeof(group_info_t));
    if (groups == NULL && group_count > 0) jc_oom("printmatches() groups");

    scan = files;
    while (scan != NULL) {
      if (ISFLAG(scan->flags, FF_HAS_DUPES)) {
        uintmax_t count = 1;
        tmpfile = scan->duplicates;
        while (tmpfile != NULL) {
          count++;
          tmpfile = tmpfile->duplicates;
        }
        groups[idx].head = scan;
        groups[idx].count = count;
        groups[idx].total_bytes = (uintmax_t)scan->size * count;
        total_bytes += groups[idx].total_bytes;
        total_files += count;
        idx++;
      }
      scan = scan->next;
    }

    if (group_count > 1) qsort(groups, group_count, sizeof(group_info_t), group_sort_cmp);

    if (pretty) {
      char total_buf[32];
      format_bytes(total_bytes, total_buf, sizeof(total_buf));
      printf("\nDuplicate Scan\n\n");
      printf("⚙ Groups: %" PRIuMAX " | Files: %" PRIuMAX " | Total: %s\n\n", (uintmax_t)group_count, total_files, total_buf);
    }

    for (idx = 0; idx < group_count; idx++) {
      printed = 1;
      if (pretty) {
        print_group_pretty(&groups[idx], (int)idx + 1);
        if (idx + 1 < group_count) printf("\n");
      } else {
        print_group(groups[idx].head, cr);
        if (idx + 1 < group_count) jc_fwprint(stdout, "", cr);
      }
    }

    free(groups);
  } else {
    if (pretty) {
      printf("\nDuplicate Scan\n\n");
      printf("➤ Match groups\n");
    }
    int group_index = 0;
    while (files != NULL) {
      if (ISFLAG(files->flags, FF_HAS_DUPES)) {
        printed = 1;
        if (pretty) {
          group_info_t group = { files, 0, 0 };
          uintmax_t count = 1;
          tmpfile = files->duplicates;
          while (tmpfile != NULL) {
            count++;
            tmpfile = tmpfile->duplicates;
          }
          group.count = count;
          group_index++;
          print_group_pretty(&group, group_index);
          if (files->next != NULL) printf("\n");
        } else {
          print_group(files, cr);
          if (files->next != NULL) jc_fwprint(stdout, "", cr);
        }
      }

      files = files->next;
    }
  }

  if (printed == 0) {
    if (pretty) printf("✓ No duplicates found\n");
    else printf("%s", s_no_dupes);
  }

  return;
}


/* Print files that have no duplicates (unique files) */
void printunique(file_t *files)
{
  file_t *chain, *scan;
  int printed = 0;
  int cr = 1;

  LOUD(fprintf(stderr, "print_uniques: %p\n", files));

  if (ISFLAG(a_flags, FA_PRINTNULL)) cr = 2;

  scan = files;
  while (scan != NULL) {
    if (ISFLAG(scan->flags, FF_HAS_DUPES)) {
      chain = scan;
      while (chain != NULL) {
        SETFLAG(chain->flags, FF_NOT_UNIQUE);
	chain = chain->duplicates;
      }
    }
    scan = scan->next;
  }

  while (files != NULL) {
    if (!ISFLAG(files->flags, FF_NOT_UNIQUE)) {
      printed = 1;
      if (ISFLAG(a_flags, FA_SHOWSIZE)) printf("%" PRIdMAX " byte%c each:\n", (intmax_t)files->size,
          (files->size != 1) ? 's' : ' ');
      jc_fwprint(stdout, files->d_name, cr);
    }
    files = files->next;
  }

  if (printed == 0) jc_fwprint(stderr, "No unique files found.", 1);

  return;
}
