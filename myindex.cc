#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <gmime/gmime.h>
#include <string.h>

#include "tokenizer.h"
#include "xapianglue.h"
using namespace std;

static void freek(gpointer k, gpointer v, gpointer d)
{
  g_free(k);
}

time_t start_time;

int main(int argc, char** argv)
{
  GMimeStream *stream;
  GMimeParser *parser;
  GMimeMessage *msg = 0;
  GHashTable* idset;
  gpointer a,b;
  time_t date;
  struct tm tm;

  size_t offset, oldoffs;
  const char *xref;
  int fh;
  

  tokenizer_init();
  xapian_init();
  start_time = time(NULL);

  // g_mime_init(GMIME_INIT_FLAG_UTF8);
  cout << argc << "  args" << endl;
  
  int argi;
  for (argi = 1; argi<argc; argi++) {
    // "/org/lists.debian.org/lists/debian-project/2007/debian-project-200709"
    string fn(argv[argi]);
    fh = open(fn.c_str(), O_RDONLY);
  
    string basename = fn.substr(fn.find_last_of('/')+1);
    int i = basename.find_last_of('-');
    string list = basename.substr(0,i);
    string yearmonth = basename.substr(i+1);
    int year = atoi(yearmonth.substr(0,4).c_str());
    int month = 0;
    if (yearmonth.length()>4)
      month = atoi(yearmonth.substr(4).c_str());

    cout << endl << list << " " << year << " ";
    if (month != 0)
      cout << month;
    cout << endl;
    idset = g_hash_table_new(g_str_hash, g_str_equal);
    int msgnum = 0;
    
    stream = g_mime_stream_fs_new(fh);

    memset(&tm, 0, sizeof(tm));
    parser = g_mime_parser_new_with_stream(stream);
    g_mime_parser_set_scan_from(parser, TRUE);
    while (! g_mime_parser_eos(parser)) //((msg != 0) && (oldoffs != offset))
      {
       
	msg = g_mime_parser_construct_message(parser);
        
	if (msg != 0)
	  {
	       
            xref = g_mime_message_get_header(msg, "Message-Id");
            if (!xref) {
              fprintf(stderr, "\nNo msgid\n");
            }
            else if (g_hash_table_lookup_extended(idset, xref, &a, &b))
              {
                fprintf(stdout, "\ndupemsgid: %s\n",xref);		  
              }
            else
              {
                // fprintf(stdout, "msgid: %s\n",xref);
		cout << "." << flush;
                g_hash_table_insert(idset, g_strdup(xref), NULL);
                document * doc = parse_article(msg);
                if (doc != NULL) {
               
                  xapian_add_document(doc, list, year, month, msgnum);

                }
                msgnum++;
             
              }
            g_object_unref(msg);
          }     }
  
    g_hash_table_foreach(idset, freek, NULL);
    g_mime_stream_unref(stream);
    close(fh);
    xapian_flush();
  }
  
  tokenizer_fini();
  printf("DONE\n");

}
