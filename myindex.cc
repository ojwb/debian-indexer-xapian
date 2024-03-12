#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <gmime/gmime.h>
#include <string.h>
#include <set>
#include <fstream>

#include "tokenizer.h"
#include "xapianglue.h"
#include "util.h"
using namespace std;

string msgid_strip(string aline)
{
   size_t l = aline.find_first_of('<');
   size_t r = aline.find_last_of('>');
   if (l<r)
     return aline.substr(l+1,r-l-1);
   return aline;
}

int main(int argc, char** argv)
{
  GMimeStream *stream;
  GMimeParser *parser;
  GMimeMessage *msg = 0;

  int fh;

  size_t unflushed_messages = 0;
  size_t flush_interval = 100000;
  bool regenerate = false;
  char *dbpathprefix = NULL;
    
  int argi;
  for (argi = 1; (argi<argc) && (strcmp(argv[argi],"-v")==0); argi++) {
    verbose += 1;
  }
  if ((argi < argc) && (strcmp(argv[argi],"--dbname")==0)) {
    ++argi;
    if (argi < argc) {
      dbpathprefix = argv[argi];
      ++argi;
    }
    else {
      cerr << "missing argument after --dbname" << endl;
    }
  }

  tokenizer_init();
  xapian_init(dbpathprefix);
  start_time = time(NULL);

  // cout << argc << "  args" << endl;
  set<string> spamids;
  set<string> seenids;
  
  enum {
    NEXT_NOTHING = 0,
    NEXT_LANG,
    NEXT_FLUSHINTERVAL,
    NEXT_DBNAME
  } whatsnext = NEXT_NOTHING;

  // argi inited above
  for (; argi<argc; argi++) {
    // "/srv/lists.debian.org/lists/debian-project/2007/debian-project-200709"
    string fn(argv[argi]);
    if (whatsnext == NEXT_LANG) {
      xapian_set_stemmer(fn);
      if (verbose != 0)
        cout << "language: " << fn << endl;
      whatsnext = NEXT_NOTHING;
      continue;
    }
    else if (whatsnext == NEXT_FLUSHINTERVAL) {
      flush_interval = atoll(fn.c_str());
      if (verbose != 0)
        cout << "flush interval: " << flush_interval << endl;
      whatsnext = NEXT_NOTHING;
      continue;
    }
    
    if (fn == "-v") {
      verbose += 1;
      continue;
    }
    if (fn == "-l") {
      whatsnext = NEXT_LANG;
      continue;
    }
    if (fn == "-f") {
      whatsnext = NEXT_FLUSHINTERVAL;
      continue;
    }
    if (fn == "-F") {
      regenerate = true;
      if (verbose > 0)
        cout << "readding existing documents" << endl; 
      continue;
    }
    
    fh = open(fn.c_str(), O_RDONLY);
    string basename = fn.substr(fn.find_last_of('/')+1);
    int lasthavemsgnum = xapian_open_db_for_month(basename, regenerate);
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

    spamids.clear();
    string spamfn = fn+".spam";
    ifstream spamf(spamfn.c_str());
    if (spamf) {
       string aline;
       while (getline(spamf, aline)) {
	  string::iterator i = aline.begin();
	  while (i != aline.end() && *i != ':') {
	     if (isalnum((unsigned char)*i) || strchr(".-+*", *i)) {
		*i = tolower(*i);
		i++;
	     } else {
		aline.erase(i);
	     }
	  }
	  if (aline.substr(0,21) == "skip-spam-message-id:") {
	    aline.erase(0,21);
	    i = aline.begin();
	    while (i != aline.end() && *i == ' ')
	      aline.erase(i);
	    spamids.insert(msgid_strip(aline));
	  }
       }
    } 
    // cout << "number spam msgids: " << spamids.size() << endl;
    seenids.clear();

    int msgnum = 0;
    
    stream = g_mime_stream_fs_new(fh);

    parser = g_mime_parser_new_with_stream(stream);
    g_mime_parser_set_scan_from(parser, TRUE);
    gint64 old_pos = -1;
    while (! g_mime_parser_eos(parser)) {
      msg = g_mime_parser_construct_message(parser);
      if (msg == 0) {
	gint64 pos = g_mime_parser_tell(parser);
	cerr << "g_mime_parser_construct_message(parser) returned NULL at offset " << pos << endl;
	if (pos == old_pos) {
	  cerr << "Giving up on '" << fn << "' - GMimeParser is stuck at offset " << pos << endl;
	  break;
	}
	old_pos = pos;
	continue;
      }

      const char* raw_msgid = g_mime_object_get_header(GMIME_OBJECT(msg), "Message-Id");
      string msgid;
      if (raw_msgid != NULL)
	msgid = msgid_strip(raw_msgid);
      else
	msgid = fake_msgid(msg);
      if (verbose >= 2)
	cerr << endl << "msgid: " << msgid << endl;
      if (msgid == "") {
	cerr << endl << "No msgid" << endl;
      }
      else if (seenids.find(msgid) != seenids.end()) {
	if (verbose > 1)
	  cerr << endl << "dupemsgid: " << msgid << endl;
      }
      else if (spamids.find(msgid) != spamids.end()) {
	if (verbose > 1)
	  cerr << endl << "spam: " << msgid << endl;
	xapian_delete_document(list, year, month, msgnum);
	seenids.insert(msgid);
	msgnum++;
      }
      else {
	if (verbose > 2)
	  cerr << endl << "msgid: " << msgid << endl;
	if (verbose > 0)
	  cout << "." << flush;
	seenids.insert(msgid);
	if ((msgnum > lasthavemsgnum) || regenerate) {
	  document * doc = parse_article(msg);
	  if (doc != NULL) {
	    xapian_add_document(doc, msgid, list, year, month, msgnum);
	    unflushed_messages++;
	  }
	}
	msgnum++;
      }
      g_object_unref(msg);
    }
     
    g_object_unref(parser);
    g_object_unref(stream);
    close(fh);
    if (unflushed_messages>flush_interval) {
      if (verbose > 0)
        cout << endl << "flushing..." << flush;
      xapian_flush();
      if (verbose > 0)
        cout << " flushed" << endl;
      unflushed_messages = 0;
    }
  }
  if (unflushed_messages>0)
     xapian_flush();
  
  tokenizer_fini();
  if (verbose != 0)
    cout << endl << "DONE" << endl;

}
