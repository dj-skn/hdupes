/* Print summary of match statistics to stdout
 * This file is part of hdupes; see hdupes.c for license information */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "hdupes.h"
#include "act_summarize.h"

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

void summarizematches(const file_t * restrict files)
{
  unsigned int numsets = 0;
  off_t numbytes = 0;
  int numfiles = 0;
  int pretty = 1;
  const char *divider = "======================================================================";

  LOUD(fprintf(stderr, "summarizematches: %p\n", files));
  if (ISFLAG(a_flags, FA_PRINTNULL)) pretty = 0;

  while (files != NULL) {
    file_t *tmpfile;

    if (ISFLAG(files->flags, FF_HAS_DUPES)) {
      numsets++;
      tmpfile = files->duplicates;
      while (tmpfile != NULL) {
        numfiles++;
        numbytes += files->size;
        tmpfile = tmpfile->duplicates;
      }
    }
    files = files->next;
  }

  if (pretty) {
    char total_buf[32];
    format_bytes((uintmax_t)numbytes, total_buf, sizeof(total_buf));
    printf("\n%s\n", divider);
    if (numsets == 0) {
      printf("✓ No duplicates found\n");
    } else {
      printf("Scan complete\n");
      printf("Potential space: %s | Items: %d | Groups: %u\n", total_buf, numfiles, numsets);
    }
    printf("%s\n", divider);
  } else {
    if (numsets == 0)
      printf("%s", s_no_dupes);
    else
    {
      printf("%d duplicate files (in %d sets), occupying ", numfiles, numsets);
      if (numbytes < 1000) printf("%" PRIdMAX " byte%c\n", (intmax_t)numbytes, (numbytes != 1) ? 's' : ' ');
      else if (numbytes <= 1000000) printf("%" PRIdMAX " KB\n", (intmax_t)(numbytes / 1000));
      else printf("%" PRIdMAX " MB\n", (intmax_t)(numbytes / 1000000));
    }
  }
  return;
}
