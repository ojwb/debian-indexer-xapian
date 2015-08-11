CPPFLAGS = $(shell pkg-config --cflags gmime-2.6)
#CPPFLAGS += $(shell xapian-config --cxxflags)
LIBS = $(shell pkg-config --libs gmime-2.6)
LIBS += $(shell xapian-config --libs)
LIBS += -lgcrypt
CXXFILES = xapianglue myindex tokenizer util
OFILES = $(CFILES:=.o) $(CXXFILES:=.o)
  CPPFLAGS += -DOLDGMIME

all: myindex

clean:
	-rm myindex *.o

myindex: $(OFILES)
	$(CC) -g -o myindex $(LIBS) $(OFILES)
