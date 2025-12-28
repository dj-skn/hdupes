/* hdupes action for deleting duplicate files
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef NO_DELETE

#ifndef ACT_DELETEFILES_H
#define ACT_DELETEFILES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hdupes.h"
extern void deletefiles(file_t *files, int prompt, FILE *tty);

#ifdef __cplusplus
}
#endif

#endif /* ACT_DELETEFILES_H */

#endif /* NO_DELETE */
