/* Output styling helpers
 * This file is part of hdupes; see hdupes.c for license information */

#include "style.h"
#include <stdlib.h>
#include <unistd.h>

static int want_color(FILE *stream)
{
  const char *no_color = getenv("NO_COLOR");
  if (no_color != NULL && *no_color != '\0') return 0;
  return isatty(fileno(stream)) != 0;
}

const char *hdupes_color(FILE *stream, const char *code)
{
  if (want_color(stream)) return code;
  return "";
}
