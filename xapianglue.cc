#include <xapian.h>

#define INDEX_CHUNK_SIZE 1000000

extern "C" {
#include "tokenizer.h"
#include "util.h"
#include <glob.h>  
}

#include "xapianglue.h"

//#include "indextext.h"

#include <string>
#include <map>
#include <iostream>

using namespace std;

Xapian::WritableDatabase db;
Xapian::Document * doc = NULL;
Xapian::TermGenerator indexer;

string index_globmask("/org/lists.debian.org/xapian/data/listdb*");

const char *index_dir = "/org/lists.debian.org/xapian/data/listdb"; // FIXME

static int counter = 0;
static string language("english");


extern "C" void
xapian_flush(void)
{
    try {
	db.flush();
    } catch (const Xapian::Error &e) {
	merror(e.get_msg().c_str());
    }
}

extern "C" void
xapian_new_document(void)
{
    if (doc != NULL)
	delete doc;
	// merror("xapian_new_document called when document is already active");
    try {
	doc = new Xapian::Document();
    } catch (const Xapian::Error &e) {
	merror(e.get_msg().c_str());
    }
}

extern "C" void
xapian_tokenise(const char* prefix, const char* text, int len)
{
    if (doc == NULL) {
	merror("xapian_tokenise called before xapian_new_document");
    }
    try {
      indexer.set_document(*doc);
	if (verbose>=2) {
	    printf("index:[");
	    fwrite(text, 1, len, stdout);
	    printf("]\n");
	}
        indexer.index_text(text, 1,  prefix ? prefix : "");
        
    } catch (const Xapian::Error &e) {
	merror(e.get_msg().c_str());
    }
}

time_t start_time;
static time_t last_start_time = 0;
static int last_total_files = 0;
static int total_files = 0;

void
xapian_delete_document(std::string & list, int year, int month, int  msgnum) 
{
   char buf[64];

   sprintf(buf, "%04d%02d%05d", year,month,msgnum);
   string ourxapid("Q");
   ourxapid += list;
   ourxapid += buf;
   db.delete_document(ourxapid);
}

void
xapian_add_document(const document *d, std::string & list, int year, int month, int  msgnum)
{
    if (doc == NULL)
	merror("xapian_add_document called before xapian_new_document");
    doc->add_term( string("G")+list);
    
    if (!d->email.empty()) {
	try {
	    doc->add_term(string("A") + d->email);
	    if (verbose >= 2) printf("author:[A%s]\n", d->email.c_str());
	} catch (const Xapian::Error &e) {
	    merror(e.get_msg().c_str());
	}
    }
    if (!d->author.empty()) {
	try {
	   xapian_tokenise("A", d->author.c_str(), d->author.length());
	    if (verbose >= 2) printf("author:[A%s]\n", d->author.c_str());
	} catch (const Xapian::Error &e) {
	    merror(e.get_msg().c_str());
	}
    }
    indexer.index_text(d->subject, 3);
    char buf[64];

    sprintf(buf, "%04d%02d%05d", year,month,msgnum);
    string ourxapid("Q");
    ourxapid += list;
    ourxapid += buf;
    doc->add_term(ourxapid);
    // G list
    // L language
    // Q id
    if (language.length()>0)
      doc->add_term(string("L")+language);

    if (month != 0)
      sprintf(buf, "%04d%02d", year,month);
    else
      sprintf(buf, "%04d", year);      
    string datecode(buf);
    doc->add_value(VALUE_DATECODE, datecode);
    doc->add_term((string("XM")+list+"-")+buf);
    
    

    if (month != 0) 
	sprintf(buf, "/%04d/%02d/msg%05d.html", year,month, msgnum);
    else 
	sprintf(buf, "/%04d/msg%05d.html", year, msgnum);
    //      $set{fieldnames,$split{url list msgno year month subject author}}
    string url("http://lists.debian.org/");
    url += list;
    url += buf;
   
    string data;
    data += url;
    data += "\n";
    data += list;
      
    sprintf(buf, "\n%d\n%d\n%d\n", msgnum, year, month);
    data += buf;
    data += d->subject;
    data += '\n';
    data += d->author;
    data += '\n';
    data += d->email;
    data += '\n';

    data += d->body;

    if (verbose >= 2) printf("data:[%s]\n\n", data.c_str());
    doc->set_data(data);
    db.replace_document(ourxapid,*doc);
    delete doc;
    doc = NULL;

    ++total_files;
    
    if ((total_files % 10000) == 0) {
	/* TV-TODO: chunks?
	 if (total_files % INDEX_CHUNK_SIZE == 0) {
	    // Move onto next database chunk...
	    xapian_init();
	}
	 */
	time_t now = time(NULL);
	time_t elapsed = now - start_time;
	if (last_start_time == 0) last_start_time = start_time;
	if (elapsed != 0) {
	    int last_elapsed = now - last_start_time;
	    int last_rate = 0;
	    if (last_elapsed)
		last_rate = (total_files - last_total_files) / last_elapsed;
            if (verbose > 0)
	      printf("    %d files (%d/s; %d/s last %d seconds)\n",
                     total_files, (int)(total_files/elapsed), last_rate,
                     last_elapsed);
	    last_start_time = now;
	    last_total_files = total_files;
	}
    }
}



extern "C" char *index_file_name(char *name) {
  static char file_name[1024];
  strcpy(file_name, index_dir);
  strcat(file_name, "/");
  strcat(file_name, name);
  return file_name;
}

map<const string, size_t> monthtodbmap;
glob_t globbuf;

void init_monthtodbmap()
{
  int res = glob(index_globmask.c_str(), 0, NULL, &globbuf);
  if (res==0) {
    for (size_t i=0; globbuf.gl_pathv[i] != NULL; i++) {
      if (verbose>0)
        cout << globbuf.gl_pathv[i] << ":" << endl;
      Xapian::Database a_db(globbuf.gl_pathv[i]);
      const string listPrefix("XM");
      for (Xapian::TermIterator ti = a_db.allterms_begin(listPrefix);
           ti != a_db.allterms_end(listPrefix);
           ti++) {
        monthtodbmap[(*ti).substr(2)] = i;
        if (verbose>0)
          cout << "  " << (*ti).substr(2) << endl;
      }
    }
    //globfree(&globbuf);
  }
  else if (res!=GLOB_NOMATCH) {
    merror("problem initializing stuff");
  }
}

extern "C" void
xapian_init(void)
{
  try {
    init_monthtodbmap();
    indexer.set_stemmer(Xapian::Stem("english")); // TV-TODO: do better
  } catch (const Xapian::Error &e) {
    merror(e.get_msg().c_str());
  }
}

static string curdb;

void xapian_open_db_for_month(const string month) 
{
  map<const string, size_t>::iterator i = monthtodbmap.find(month);
  if (i != monthtodbmap.end()) {
    if (curdb != globbuf.gl_pathv[i->second]) {
      curdb = globbuf.gl_pathv[i->second];
      db = Xapian::WritableDatabase(curdb, Xapian::DB_CREATE_OR_OPEN);
    }    
    total_files = db.get_doccount();
  }
  else {
    total_files = -1;
    while ((total_files < 0) or (total_files > INDEX_CHUNK_SIZE)) {
      string dbpath(index_dir);
      char buf[256];
      sprintf(buf, "-%03d", counter);
      dbpath += buf;
      if (curdb != dbpath) {
        curdb = dbpath;
        db = Xapian::WritableDatabase(dbpath, Xapian::DB_CREATE_OR_OPEN);
      }
      total_files = db.get_doccount();
      if (total_files > INDEX_CHUNK_SIZE)
        counter++;
    }
  }  
}

void xapian_set_stemmer(const string lang)
{
  if (lang == language) {
    return;    
  }
  
  try { 
    string languages(" ");
    languages += Xapian::Stem::get_available_languages();
    languages += " ";
    if ((lang.length() == 0) || (languages.find(lang) == string::npos)) {
      indexer.set_stemmer(Xapian::Stem());
      language = "";
    }
    else {
      indexer.set_stemmer(Xapian::Stem(lang));
      language = lang;
    }
  } catch (const Xapian::Error &e) {
    merror(e.get_msg().c_str());
  }
}
