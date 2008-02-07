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

static string dbpathprefix("/org/lists.debian.org/xapian/data/listdb");

static int counter = 0;
static string language, stemmer_language;

void xapian_flush(void)
{
    try {
	db.flush();
    } catch (const Xapian::Error &e) {
	merror(e.get_msg().c_str());
    }
}

void xapian_new_document(void)
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

void xapian_tokenise(const char* prefix, const char* text, int len)
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
    // XSL language used for stemming
    // Q id
    if (! language.empty())
      doc->add_term(string("L")+language);
    if (! stemmer_language.empty())
      doc->add_term(string("XSL")+stemmer_language);    

    if (month != 0)
      sprintf(buf, "%04d%02d", year,month);
    else
      sprintf(buf, "%04d", year);      
    doc->add_term((string("XM")+list+"-")+buf);
    
    struct tm ts;
    memset(&ts, 0, sizeof(ts));
    ts.tm_year = year-1900;
    ts.tm_mon = (month != 0 ? month-1 : 0);
    ts.tm_mday = 1;
    time_t t = mktime(&ts);
    if (d->date + 3*24*3600 >= t) {
      // this works for monthly and yearly lists
      ts.tm_year += (month%12)==0;
      ts.tm_mon = (month%12);
      t = mktime(&ts);
      if (t + 3*24*3600 >= d->date) {
        t = d->date;
      }
      else {
        if (verbose > 0)
          cerr << "date header out of bounds in message " << ourxapid << endl;        
      }
    }
    else {
      if (verbose > 0)
        cerr << "date header out of bounds in message " << ourxapid << endl;
    }
    gmtime_r(&t, &ts);
    strftime(buf, sizeof(buf), "%Y-%m-%d-%H-%M", &ts);
    doc->add_value(VALUE_DATECODE, buf);

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

map<const string, size_t> monthtodbmap;
glob_t globbuf;

void init_monthtodbmap()
{
  int res = glob((dbpathprefix+"*").c_str(), 0, NULL, &globbuf);
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

void xapian_init(const char* adbpathprefix)
{
  if (adbpathprefix) {
    dbpathprefix = adbpathprefix;
  }
  
  try {
    init_monthtodbmap();
    xapian_set_stemmer("en");
  } catch (const Xapian::Error &e) {
    merror(e.get_msg().c_str());
  }
}

static string curdb;

long xapian_open_db_for_month(const string month, const bool deleteallexisting)
{
  map<const string, size_t>::iterator i = monthtodbmap.find(month);
  int maxmsgnum = -1;
  if (i != monthtodbmap.end()) {
    if (curdb != globbuf.gl_pathv[i->second]) {
      curdb = globbuf.gl_pathv[i->second];
      db = Xapian::WritableDatabase(curdb, Xapian::DB_CREATE_OR_OPEN);
    }    
    if (! deleteallexisting) {
      // get last message indexed, stupid duplication...
      int i = month.find_last_of('-');
      string prefix(string("Q")+month.substr(0,i)+month.substr(i+1));

      if (verbose>=2)
        cout << "looking for documents beginning with " << prefix << endl;
    
      for (Xapian::TermIterator ti = db.allterms_begin(prefix);
           ti != db.allterms_end(prefix);
           ti++) {
        int msgnum = atoi((*ti).substr((*ti).length()-5).c_str());
        if (msgnum>maxmsgnum)
          maxmsgnum = msgnum;
      }
      if (verbose>=2)
        cout << "have indexed " << month <<  " up to " << maxmsgnum << endl;
    }
    else {
      if (verbose > 0)
        cout << "deleting documents from " << month << endl;
      db.delete_document(string("XM")+month);
    }
    total_files = db.get_doccount();
  }
  else {
    total_files = -1;
    while ((total_files < 0) or (total_files > INDEX_CHUNK_SIZE)) {
      string dbpath(dbpathprefix);
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
  return maxmsgnum;
}

void xapian_set_stemmer(const string lang)
{
  if (lang == language) {
    return;    
  }
  
  try {
    language = lang;
    try {
      indexer.set_stemmer(Xapian::Stem(lang));
      stemmer_language = lang;
    } catch (const Xapian::InvalidArgumentError &e) {
      indexer.set_stemmer(Xapian::Stem());
      stemmer_language.erase();
    }
  } catch (const Xapian::Error &e) {
    merror(e.get_msg().c_str());
  }
}
