/* Experimental array/group matching pipeline
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_PIPELINE_H
#define HDUPES_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hdupes.h"

int pipeline_match_files(file_t *files, ordertype_t ordertype);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_PIPELINE_H */
