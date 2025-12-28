/* hdupes extended filters
 * See hdupes.c for license information */

#ifndef HDUPES_EXTFILTER_H
#define HDUPES_EXTFILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NO_EXTFILTER

#include "hdupes.h"

void add_extfilter(const char *option);
int extfilter_exclude(file_t * const restrict newfile);

#endif /* NO_EXTFILTER */

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_EXTFILTER_H */
