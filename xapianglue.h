
#ifdef __cplusplus
extern "C" {
#endif

enum value_slot {
  VALUE_DATECODE = 0
};

void xapian_init(void);
void xapian_flush(void);
void xapian_new_document(void);
void xapian_tokenise(const char* prefix, const char* text, int len);

extern const char *index_dir;
extern time_t start_time;

char *index_file_name(char *name);

#ifdef __cplusplus
}

#include <string>

void xapian_add_document(const document *d, std::string & list, int year, int month, int msgnum);
void xapian_delete_document(std::string & list, int year, int month, int  msgnum);
void xapian_set_stemmer(const std::string lang);
long xapian_open_db_for_month(const std::string month, const bool deleteallexisting);


#endif

