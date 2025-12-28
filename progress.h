/* hdupes argument functions
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_PROGRESS_H
#define HDUPES_PROGRESS_H

#ifdef __cplusplus
extern "C" {
#endif

void update_phase1_progress(const char * const restrict type);
void update_phase2_progress(const char * const restrict msg, const int file_percent);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_PROGRESS_H */
