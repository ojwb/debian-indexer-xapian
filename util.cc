#include "util.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <iostream>
#include <gcrypt.h>

using namespace std;

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

static bool have_inited_gnutls = false;
static char hexchars[] = "0123456789abcdef";

// mhonarc-style msgid
string fake_msgid(GMimeMessage* msg) 
{
   gcry_md_hd_t md5;
   string res;
   
   if (! have_inited_gnutls) {
      gcry_control( GCRYCTL_DISABLE_SECMEM_WARN );
      gcry_control( GCRYCTL_INIT_SECMEM, 16384, 0 );
      have_inited_gnutls = true;
   }
   gcry_md_open(&md5, GCRY_MD_MD5,0);
   char* headers = g_mime_message_get_headers(msg);
   size_t l = strlen(headers);
   gcry_md_write(md5,headers,strlen(headers)); /*<-- this should create the checksum*/
   free(headers);
   gcry_md_final( md5 );
   unsigned char *digest = gcry_md_read( md5, GCRY_MD_MD5 );
   for (int i=0; i<gcry_md_get_algo_dlen( GCRY_MD_MD5 ); i++ ) {
      res += hexchars[digest[i] >> 4];
      res += hexchars[digest[i] & 0xF];
   }
   gcry_md_close(md5);
   res += "@NO-ID-FOUND.mhonarc.org";
   return res;
  
}
