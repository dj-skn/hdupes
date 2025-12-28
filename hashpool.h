/* Parallel hashing support
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_HASHPOOL_H
#define HDUPES_HASHPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hdupes.h"

int prehash_files(file_t *files, unsigned int threads);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_HASHPOOL_H */
