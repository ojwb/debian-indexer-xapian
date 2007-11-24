
#ifdef __cplusplus
extern "C" {
#endif

void xapian_init(void);
void xapian_flush(void);
void xapian_new_document(void);
void xapian_tokenise(const char* prefix, const char* text, int len);

extern const char *index_dir;
char *index_file_name(char *name);

#ifdef __cplusplus
}

#include <string>

void xapian_add_group_filter(const std::string & group);
void xapian_add_document(const document *d, std::string & list, int year, int month, int msgnum);

#endif

