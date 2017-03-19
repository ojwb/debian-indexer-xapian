
enum value_slot {
  VALUE_DATECODE = 0
};

extern void xapian_init(const char* dbpathprefix);
extern void xapian_flush(void);
extern void xapian_new_document(void);
extern void xapian_tokenise(const char* prefix, const char* text, int len);

extern const char *index_dir;
extern time_t start_time;

#include <string>

void xapian_add_document(const document *d, std::string & msgid, std::string & list, int year, int month, int msgnum);
void xapian_delete_document(std::string & list, int year, int month, int  msgnum);
void xapian_delete_msgid(std::string & msgid);
void xapian_set_stemmer(const std::string lang);
long xapian_open_db_for_month(const std::string month, const bool deleteallexisting);
