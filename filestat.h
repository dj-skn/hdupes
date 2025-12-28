/* hdupes file/dir stat()-related functions
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_FILESTAT_H
#define HDUPES_FILESTAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hdupes.h"

int file_has_changed(file_t * const restrict file);
int getfilestats(file_t * const restrict file);
/* Returns -1 if stat() fails, 0 if it's a directory, 1 if it's not */
int getdirstats(const char * const restrict name,
		hdupes_ino_t * const restrict inode, dev_t * const restrict dev,
		hdupes_mode_t * const restrict mode);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_FILESTAT_H */
