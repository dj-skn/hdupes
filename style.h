/* Output styling helpers
 * This file is part of hdupes; see hdupes.c for license information */

#ifndef HDUPES_STYLE_H
#define HDUPES_STYLE_H

#include <stdio.h>

#define HDUPES_MAGENTA "\033[1;35m"
#define HDUPES_YELLOW  "\033[0;33m"
#define HDUPES_GREEN   "\033[0;32m"
#define HDUPES_BLUE    "\033[0;34m"
#define HDUPES_RESET   "\033[0m"

const char *hdupes_color(FILE *stream, const char *code);

#endif /* HDUPES_STYLE_H */
