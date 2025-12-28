/* hdupes argument functions
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_ARGS_H
#define HDUPES_ARGS_H

#ifdef __cplusplus
extern "C" {
#endif

char **cloneargs(const int argc, char **argv);
int findarg(const char * const arg, const int start, const int argc, char **argv);
int nonoptafter(const char *option, const int argc, char **oldargv, char **newargv);
void linkfiles(file_t *files, const int linktype, const int only_current);

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_ARGS_H */
