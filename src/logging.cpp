
#include "logging.h"
#include <stdarg.h>

void log(const char *format, ... ) {
  va_list arglist;
  va_start(arglist, format);
  vprintf(format, arglist);
  va_end(arglist);
  fflush(stdout);
}


void log_warn(const char *format, ... ) {
  printf("WARNING -->  ");
  va_list arglist;
  va_start(arglist, format);
  vprintf(format, arglist);
  va_end(arglist);
  fflush(stdout);
}
 

void log_error(const char *format, ... ) {
  printf("ERROR --> ");
  va_list arglist;
  va_start(arglist, format);
  vprintf(format, arglist);
  va_end(arglist);
  fflush(stdout);
}

