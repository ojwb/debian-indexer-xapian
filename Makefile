CPPFLAGS = $(shell gmime-config --cflags)
CPPFLAGS += $(shell xapian-config --cxxflags)
LIBS = $(shell gmime-config --libs)
LIBS += $(shell xapian-config --libs)

CXXFILES = xapianglue myindex tokenizer
CFILES = util
OFILES = $(CFILES:=.o) $(CXXFILES:=.o)

all: myindex

clean:
	-rm myindex *.o

myindex: $(OFILES)
	gcc -o myindex $(LIBS) $(OFILES)