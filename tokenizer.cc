/*
  The purpose of this program is to decode text parts of a message
  and remove all attachments before running the results into a
  search engine indexer.

  The function to use is parse_article().  It returns a static
  document structure, which has the author, subject, newsgroup, article
  number and time, as well as a list of word structures.  Each word
  structure has a string and a count.

  The caller doesn't have to allocate or free any structures.  They
  are all defined staticly in this file.

  A process should call 

    tokenizer_init();

  once before calling the parse_article() function.  This will set up the
  proper structures for downcasing Latin-1, and for using English stop
  words.

  This tokenizer has been tested with gmime 2.1.15 and glib 2.0.
 */

#include "tokenizer.h"
#include "xapianglue.h"
#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <map>
#include <set>
#include <string>
using namespace std;

#ifndef O_STREAMING
# ifdef __linux__
// This is the value used by rml's O_STREAMING patch for 2.4.
#  define O_STREAMING	04000000
# else
// Define as 0 otherwise, so we don't need ifdefs in the code.
#  define O_STREAMING	0
# endif
#endif

static map<string, string> charsets;
static set<string> groups;

// FIXME:
// Need to scan gmane.conf and find the line ":groupname:", then look for
// :charset=([^:]*) and set default_charset to $1 if found.
static const char * default_charset = NULL;

static document doc;
static int tallied_length;

static int doc_body_length = 0;

/* Save some text from the body of a message.  The idea here is that
   we ignore all lines that start with ">" to avoid saving bits of
   quoted text. */
static void save_body_bits(const char *text, int start, int end) {
  char c;
  int i;
  int nl = 1;
  int save = 1;
  int whitespace = (doc_body_length == 0); /* Ignore leading whitespace */
  char last_c = 0;
  int last_c_count = 0;
  int doc_body_length_old = doc_body_length;
  
  for (i = start; i<end; i++) {
    c = text[i];
    if (nl && c == '>') {
      save = 0;
      // Try to avoid picking up attribution lines before quoted text...
      if (doc_body_length_old >= 0) {
	  doc_body_length = doc_body_length_old;
	  whitespace = (doc_body_length == 0);
	  doc_body_length_old = -1;
      }
    }
    if (save) {
      if (doc_body_length == MAX_SAVED_BODY_LENGTH - 1) return;
      if (isspace((unsigned char)c)) {
	/* Collapse runs of whitespace to a single space character. */
	if (!whitespace) doc.body[doc_body_length++] = ' ';
	whitespace = 1;
      } else if (!isalnum((unsigned char)c) && c == last_c) {
	/* Collapse long runs like "---------", ".......", "========"
	 * to a run of just 3 of the same character. */
	if (++last_c_count < 3) doc.body[doc_body_length++] = c;
	whitespace = 0;
      } else {
	doc.body[doc_body_length++] = c;
	whitespace = 0;
      }
    }
    if (c == '\n') {
      nl = 1;
      save = 1;
    } else {
      nl = 0;
    }
    if (c != last_c) {
      last_c = c;
      last_c_count = 0;
    }
  }
}

static void tally(const char* itext, int start, int end) {
  if ((tallied_length + end - start) >= MAX_MESSAGE_SIZE) {
    fprintf(stderr, "Max message size reached.\n");
    // TV-COMMENT: should print msgid
    return;
  }
  xapian_tokenise(NULL, itext + start, end - start);
  tallied_length += end - start;
}

static void tally_string(const char *string) {
  tally(string, 0, strlen(string));
}

static void transform_text_plain(const char *content) {
  if (content == NULL) return;
  if (strncmp(content, "-----BEGIN PGP SIGNED MESSAGE-----", 34) == 0) {
      /* No need to unquote lines - the indexing will just ignore it */
      const char *s;
      content += 34;
      s = strstr(content, "\n\n");
      if (s != NULL) content = s + 2;
      /* Now remove the trailer. */
      if ((s = strstr(content, "-----BEGIN PGP SIGNATURE-----")) != NULL) {
	  tally(content, 0, s - content);
	  save_body_bits(content, 0, s - content);
	  return;
      }
  }
  tally_string(content);
  save_body_bits(content, 0, strlen(content));
}

static void transform_text_html(const char *content) {
  gchar curChar = 0;
  size_t beg = 0;
  size_t contentLen = strlen(content);
  size_t i;
  // FIXME : parse HTML better - entities are the main issue, but
  // also not all tags count as space!
  for (i=0; i<contentLen; ++i) {
    curChar = content[i];

    if (curChar == '<') {
      if (i != beg) {
	tally(content, beg, i);
	save_body_bits(content, beg, i);
	beg = i+1;
      }
    }

    if (curChar == '>') 
      beg = i+1;
  }
  if (i != beg) {
    tally(content, beg, i);
    save_body_bits(content, beg, i);
  }
}

static void transform_message_rfc822(const char *content);

typedef struct transform {
  const char *content_type;
  void (*function)(const char *);
} transform;

static struct transform part_transforms[] = {
  {"text/plain", transform_text_plain},
  {"text/html", transform_text_html},
  {"message/rfc822", transform_message_rfc822},
  {NULL, NULL}};

static char *convert_to_utf8(const char *string, const char *charset) {
  const char *utf8, *local;
  iconv_t local_to_utf8;
  char *result;

  utf8 = g_mime_charset_name("utf-8");
  local = g_mime_charset_name(charset);
  local_to_utf8 = g_mime_iconv_open(utf8, local);
  result = g_mime_iconv_strdup(local_to_utf8, string);
  g_mime_iconv_close(local_to_utf8);

  return result;
}

static void transform_simple_part(GMimePart* part) {
//    fprintf(stderr, "transform_simple_part\n");
  const GMimeContentType* ct = 0;
  const gchar* content = 0;
  size_t contentLen = 0;
  int i = 0;
  char content_type[128];
  const char *part_type;
  char *mcontent, *p, *ccontent = NULL, *use_content;
  const char *charset = NULL;

  ct = g_mime_part_get_content_type(part);

  if (ct == NULL ||
      ct->type == NULL ||
      ct->subtype == NULL) {
    strcpy(content_type, "text/plain");
  } else {
    charset = g_mime_content_type_get_parameter(ct, "charset");
    snprintf(content_type, sizeof(content_type), "%s/%s", 
	     ct->type, ct->subtype);
  }

  if (charset == NULL) {
    charset = default_charset;
  }

  for (p = content_type; *p; p++) 
    *p = tolower(*p);

  content = g_mime_part_get_content(part, &contentLen);
  /* We copy over the content and zero-terminate it. */
  mcontent = (char *)malloc(contentLen + 1);
  memcpy(mcontent, content, contentLen);
  *(mcontent + contentLen) = 0;

  /* Convert contents to utf-8.  If the conversion wasn't successful,
     we use the original contents. */
  if (strcmp(charset, "utf-8")) 
    ccontent = convert_to_utf8(mcontent, charset);

  if (ccontent != NULL)
    use_content = ccontent;
  else
    use_content = mcontent;

  for (i = 0; ; i++) {
    if ((part_type = part_transforms[i].content_type) == NULL) {
      if (0 && strncmp(content_type, "text/", 5)) {
	transform_text_plain(use_content);
      }
      break;
    } else if (! strcmp(part_type, content_type)) {
      (part_transforms[i].function)(use_content);
      break;
    }
  }

  free(mcontent);
  if (ccontent != NULL)
    free(ccontent);
}

static void transform_part(GMimeObject *mime_part);
 
static void
transform_multipart(GMimeMultipart *mime_part, const GMimeContentType * ct) {
//    fprintf(stderr, "transform_multipart\n");
  GList *child;
  GMimeObject *preferred = NULL;
  const char *type, *subtype = NULL;

  if (ct != NULL) 
    subtype = ct->subtype;

  if (subtype == NULL)
    subtype = "mixed";

//  fprintf(stderr, "%s/%s:\n", ct->type, subtype);
  if (! strcmp(subtype, "alternative")) {
    /* This is multipart/alternative, so we need to decide which
       part to output. */
      
    child = mime_part->subparts;
    while (child) {
      ct = g_mime_object_get_content_type((GMimeObject*)child->data);
      if (ct == NULL) {
	type = "text";
	subtype = "plain";
      } else {
	type = ct->type? ct->type: "text";
	subtype = ct->subtype? ct->subtype: "plain";
      }
//      fprintf(stderr, "  %s/%s:\n", type, subtype);
	  
      if (! strcmp(type, "multipart") ||
	  ! strcmp(type, "message")) 
	preferred = (GMimeObject*)child->data;
      else if (! strcmp(type, "text")) {
	if (! strcmp(subtype, "html"))
	  preferred = (GMimeObject*)child->data;
	else if (! strcmp(subtype, "plain") && preferred == NULL)
	  preferred = (GMimeObject*)child->data;
      }
      child = child->next;
    }

    if (! preferred) {
      /* Use the last child as the preferred. */
      child = mime_part->subparts;
      while (child) {
	preferred = (GMimeObject*)child->data;
	child = child->next;
      }
    }

    transform_part(preferred);

  } else if (! strcmp(subtype, "digest")) {
    /* multipart/digest message. */
    GMimeContentType * ct;
    child = mime_part->subparts;
    while (child) {
      if (GMIME_IS_PART(child->data)) {
//	  fprintf(stderr, "digest part\n");
	ct = g_mime_content_type_new("message", "rfc822");
	g_mime_part_set_content_type(GMIME_PART(child->data), ct);
	transform_part((GMimeObject *) child->data);
      } else {
//	  fprintf(stderr, "multipart/digest subpart isn't a GMimePart?!\n");
      }
      child = child->next;
    }
  } else {
    /* Multipart mixed and related. */
    child = mime_part->subparts;
    while (child) {
      transform_part((GMimeObject *) child->data);
      child = child->next;
    }
  }
}

static void transform_part(GMimeObject *mime_part) {
//    fprintf(stderr, "transform_part\n");
//  fprintf(stderr, "child is %s\n", g_type_name(G_TYPE_FROM_INSTANCE(mime_part)));
  if (GMIME_IS_MULTIPART(mime_part)) {
    transform_multipart(GMIME_MULTIPART(mime_part),
			g_mime_object_get_content_type(mime_part)); 
  } else if (GMIME_IS_PART(mime_part)) {
    transform_simple_part(GMIME_PART(mime_part));
  } else if (GMIME_IS_MESSAGE_PART(mime_part)) {
    GMimeMessagePart * msgpart = GMIME_MESSAGE_PART(mime_part);
    GMimeMessage * msg = g_mime_message_part_get_message(msgpart);
    transform_part(msg->mime_part);
    g_object_unref(msg);
  } else if (mime_part != NULL) {
    fprintf(stderr, "part is type %s\n", g_type_name(G_TYPE_FROM_INSTANCE(mime_part)));
  }
}

static void transform_message_rfc822(const char *content) {
  GMimeStream *stream;
  GMimeParser *parser;
  GMimeMessage *msg;
  // fprintf(stderr, "RFC822...\n");
  stream = g_mime_stream_mem_new_with_buffer(content, strlen(content));
  parser = g_mime_parser_new_with_stream(stream);
  msg = g_mime_parser_construct_message(parser);
  g_object_unref(parser);
  g_mime_stream_unref(stream);
  if (msg != 0) {
    transform_part(msg->mime_part); 
    g_mime_object_unref(GMIME_OBJECT(msg));
  }
}

int counter = 0;

//document* parse_article(FILE *fh, size_t len, time_t date, const char *email);
document* parse_article(GMimeMessage* msg) {
  //GMimeMessage *msg = 0;
  const char *xref;

  tallied_length = 0;

  //msg = g_mime_parser_construct_message(parser);

  if (msg == 0) goto dontindex;

  {
    xapian_new_document();
    default_charset = NULL; // TV-TODO
    
    if (default_charset == NULL)
      default_charset = "iso-8859-1";
    doc_body_length = 0;
    const char *from = g_mime_message_get_sender(msg);
    if (from) {
	string name = from;
	/* if (strstr(from, "=?")) {
	    char * copy = strdup(from);
	    if (!copy) {
		printf("strdup failed\n");
		exit(1);
	    }
	    char * p = copy;
	    char * start;
	    while ((start = strstr(p, "=?")) != NULL) {
		name.assign(p, start - p);
		// E.g. =?ISO-8859-1?B?6Q==?=
		p = strchr(start + 2, '?');
		if (p) p = strchr(p, '?');
		if (p) p = strchr(p, '?');
		if (!p || p[1] != '=') {
		    p = start;
		    break;
		}
		p += 2;
		char keep = *p;
		*p = '\0';
		char * decoded = g_mime_utils_header_decode_text((unsigned char *)start);
		name.append(decoded);
		*p = keep;
	    }
	    name.append(p);
	    free(copy);
	} else if (g_mime_utils_text_is_8bit((const unsigned char *)from, strlen(from))) {
	    char * utf8_from = convert_to_utf8(from, default_charset);
	    if (utf8_from) {
		name = utf8_from;
		free(utf8_from);
	    }
	}

	if (name.empty()) name = from; */

	string author;
	InternetAddressList *iaddr_list;
	if ((iaddr_list = internet_address_parse_string(name.c_str())) != NULL) {
	    InternetAddress *iaddr = iaddr_list->address;
	    if (iaddr->name) author = iaddr->name;

	    string pre = author;
	    // Convert any \" to ".
	    for (size_t i = 0; (i = author.find("\\\"", i)) != string::npos; ++i) {
		author.replace(i, 2, "\"");
	    }
	    // If the author is enclosed in (), <>, --, ==, etc remove them.
	    while (!author.empty()) {
                size_t i = 0;
                size_t j;
                while (((i = author.find("=?", i)) != string::npos) &&
                       ((j = author.find("?=", i+1)) != string::npos)) {
                  j = j-i+2;
                  if (verbose > 1)
                    cout << endl << "#BEFORE#" << author << endl;
                  char *s = g_mime_utils_header_decode_text(
#ifdef OLDGMIME
                    (const unsigned char*)
#endif
                    author.substr(i,j).c_str());
                  author.replace(i,j, s);
                  free(s);
                  if (verbose > 1)
                    cout << "#AFTER#" << author << endl;
                  i += j;
                  pre = author;
                }
              
		if (author[author.size() - 1] == ' ') {
		    author.erase(author.size() - 1);
		    continue;
		}
		if (author[0] == ' ') {
		    author.erase(0, 1);
		    continue;
		}
		const char * l = "(<-=*{[_\"%'.!:@^$|+";
		const char * r = ")>-=*}]_\"%'.!:@^$|+";
		const char * p = strchr(l, author[0]);
		if (!p || author[author.size() - 1] != r[p - l]) break;
		author.resize(author.size() - 1);
		author.erase(0, 1);
	    }
	    if (pre != author) {
		if (verbose > 0)
                  cout << "Stripped " << pre << " to " << author << endl;
	    }

	    if (!author.empty()) {
		xapian_tokenise("A", author.data(), author.size());
		if (author.size() > MAX_HEADER_LENGTH)
		    author.resize(MAX_HEADER_LENGTH);
		doc.author = author;
	    } else {
		doc.author.erase();
	    }

	    if (iaddr->type == INTERNET_ADDRESS_NAME) {
		doc.email = iaddr->value.addr;
		if (doc.email.size() > 21 &&
		    doc.email.substr(doc.email.size() - 17) == "@public.gmane.org") {
		    doc.email.resize(doc.email.size() - 16);
		} else if (!doc.email.empty() && doc.email[doc.email.size() - 1] == '@') {
		    doc.email.resize(doc.email.size() - 1);
		}
	    } else {
		internet_address_set_name(iaddr, "");
		char * address = internet_address_to_string(iaddr, FALSE);
		if (verbose > 0)
                  cout << "group email " << name << " -> " << address << endl;
		doc.email = address;
		free(address);
		if (!doc.email.empty() && doc.email[doc.email.size() - 1] == '@') {
		    doc.email.resize(doc.email.size() - 1);
		}
	    }
	    internet_address_list_destroy(iaddr_list);
	} else {
            if (verbose > 0)
              cout << "Failed to parse From: " << name << endl;
	    doc.author = name;
	    doc.email.erase();
	}
    } else {
	doc.author.erase();
	doc.email.erase();
    }

    const char * subj = g_mime_message_get_subject(msg);
    if (subj) {
      char * subject = strdup(subj);
      /* g_mime_message_get_subject decodes to UTF8 allright.
       * unless there are problems? but the below doesn't help.
       cerr << "s1:" << subj << endl;
      if (strstr(subj, "=?")) {
	subject = g_mime_utils_header_decode_text((unsigned char *) subj);
        cerr << "s2:" << subject << endl;
      } else if (g_mime_utils_text_is_8bit((const unsigned char*) subj, strlen(subj))) {
	subject = convert_to_utf8(subj, default_charset);
        cerr << "s2b:" << subject << endl;
      } else {
	subject = strdup(subj);
        cerr << "s2c:" << subject << endl;
      }
      cerr << "s3:" << subject << endl; */
      doc.subject.erase();
      if (subject) {
	unsigned char *s = (unsigned char *)subject;
	while (isspace(*s)) ++s;
	unsigned char *p = s;
	// Trim away additional "Re:" prefixes and leading whitespace.
	while (tolower(*p) == 'r' && tolower(p[1]) == 'e' && p[2] == ':') {
	  s = p;
	  p = s + 3;
	  while (isspace(*p)) ++p;
	}
	if (*s) {
	  size_t len = strlen((char *)s);
	  tally((char *)s, 0, len);
	  if (len > MAX_HEADER_LENGTH) s[MAX_HEADER_LENGTH] = 0;
	  doc.subject = (char *)s;
	}
	free(subject);
      }
    } else {
      doc.subject.erase();
    }

    {
      int gmt_offset;
      g_mime_message_get_date(msg, &doc.date, &gmt_offset);
      /* int i = (gmt_offset<0 ? -1 : 1);
      gmt_offset *= i;
      doc.date -= i*60*((gmt_offset/100*60)+(gmt_offset%100)); */
    }
    

    transform_part(msg->mime_part); 

    // g_mime_object_unref(GMIME_OBJECT(msg));
  }  
  doc.body[doc_body_length] = '\0';

  return &doc;

dontindex:
  // if (msg) g_mime_object_unref(GMIME_OBJECT(msg));   
  return NULL;
}

void tokenizer_init(void) {
  g_mime_init(GMIME_INIT_FLAG_UTF8);
}

void tokenizer_fini(void) {
  g_mime_shutdown();
}
