#include "util.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

/* Say whether a string is all-numerical. */
int is_number(const char *string) {
  while (*string)
    if (! isdigit(*string++)) 
      return 0;
  return 1;
}

void merror(const char *error) {
  perror(error);
  abort();
  // exit(1);
}
