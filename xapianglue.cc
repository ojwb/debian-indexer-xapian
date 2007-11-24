#include <xapian.h>

#define INDEX_CHUNK_SIZE 1000000

extern "C" {
#include "tokenizer.h"
#include "util.h"
}

#include "xapianglue.h"

//#include "indextext.h"

#include <string>
using namespace std;

Xapian::WritableDatabase db;
Xapian::Document * doc = NULL;
Xapian::TermGenerator indexer;

const char *index_dir = "/org/lists.debian.org/xapian/data/listdb"; // FIXME

static int counter = 0;

static bool debug = false;

extern "C" void
xapian_init(void)
{
    try {
	string dbpath(index_dir);
	/* char buf[256];
	sprintf(buf, "-%03d", counter++);
	dbpath += buf; */
       db = Xapian::WritableDatabase(dbpath, Xapian::DB_CREATE_OR_OPEN);
	const char *p = getenv("DEBUG");
	if (p && *p) debug = true;
        indexer.set_stemmer(Xapian::Stem("english")); // TV-TODO: do better
    } catch (const Xapian::Error &e) {
	merror(e.get_msg().c_str());
    }
}

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
	if (debug) {
	    printf("index:[");
	    fwrite(text, 1, len, stdout);
	    printf("]\n");
	}
        indexer.index_text(text, 1,  prefix ? prefix : "");
        
    } catch (const Xapian::Error &e) {
	merror(e.get_msg().c_str());
    }
}

void
xapian_add_group_filter(const string & group)
{
    if (doc == NULL)
	merror("xapian_add_group_filter called before xapian_new_document");
    try {
	string filter = string("G") + group;
	doc->add_term(filter);
	if (debug) printf("group:[%s]\n", filter.c_str());
	// Could add wildcarded filters here.
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
	    if (debug) printf("author:[A%s]\n", d->email.c_str());
	} catch (const Xapian::Error &e) {
	    merror(e.get_msg().c_str());
	}
    }
    if (!d->author.empty()) {
	try {
	   xapian_tokenise("A", d->author.c_str(), d->author.length());
	    if (debug) printf("author:[A%s]\n", d->author.c_str());
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

    if (debug) printf("data:[%s]\n\n", data.c_str());
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
