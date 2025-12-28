/* hdupes directory scanning code
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_LOADDIR_H
#define HDUPES_LOADDIR_H

#ifdef __cplusplus
extern "C" {
#endif

//file_t *grokfile(const char * const restrict name, file_t * restrict * const restrict filelistp);
void loaddir(char * const restrict dir, file_t * restrict * const restrict filelistp, int recurse);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_LOADDIR_H */
