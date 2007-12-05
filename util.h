#ifndef UTIL_H
#define UTIL_H

#include <gmime/gmime.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

int is_number(const char *string);
void merror(const char *error);
#ifdef __cplusplus
}
#endif

std::string fake_msgid(GMimeMessage* msg);
extern int verbose;

#endif
