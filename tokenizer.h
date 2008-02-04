#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <gmime/gmime.h>
#include <stdio.h>
#include <string.h>

/* This is the size of the internal buffer for doing message handling.
   Articles that are bigger than this will have their ends chopped off
   before they are tokenized.  */
#define MAX_MESSAGE_SIZE (1024*512)

/* Only this many characters from a header are considered. */
#define MAX_HEADER_LENGTH 80

/* How many bytes of text from the body that will be saved in the data
   base.  This is used for making pretty search results that displays
   a section of the matching article. */
#define MAX_SAVED_BODY_LENGTH 340

#ifdef __cplusplus
#include <string>

typedef struct {
  std::string author;
  std::string email;
  std::string subject;
  time_t date;
  char body[MAX_SAVED_BODY_LENGTH];
} document;

document* parse_article(GMimeMessage* msg);

//document* parse_article(FILE *fh, size_t len, time_t date, const char *email);

extern "C" {
#endif

void tokenizer_init(void);
void tokenizer_fini(void);

#ifdef __cplusplus
}
#endif

#endif
