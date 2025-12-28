/* File order sorting functions
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_SORT_H
#define HDUPES_SORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hdupes.h"

#ifndef NO_MTIME
int sort_pairs_by_mtime(file_t *f1, file_t *f2);
#endif
int sort_pairs_by_filename(file_t *f1, file_t *f2);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_SORT_H */
