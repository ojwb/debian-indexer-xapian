CPPFLAGS = $(shell gmime-config --cflags)
CPPFLAGS += $(shell xapian-config --cxxflags)
LIBS = $(shell gmime-config --libs)
LIBS += $(shell xapian-config --libs)
LIBS += -lgcrypt
CXXFILES = xapianglue myindex tokenizer util
OFILES = $(CFILES:=.o) $(CXXFILES:=.o)

all: myindex

clean:
	-rm myindex *.o

myindex: $(OFILES)
	$(CC) -g -o myindex $(LIBS) $(OFILES)