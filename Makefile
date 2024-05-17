CPPFLAGS = $(shell pkg-config --cflags gmime-2.6)
#CPPFLAGS += $(shell xapian-config --cxxflags)
LIBS = $(shell pkg-config --libs gmime-2.6)
LIBS += $(shell xapian-config --libs)
LIBS += -lgcrypt
CXXFILES = xapianglue myindex tokenizer util
OFILES = $(CFILES:=.o) $(CXXFILES:=.o)
CXXFLAGS = -Wall -W -O2 -g

all: myindex

clean:
	-rm -f myindex *.o *.pyc

myindex: $(OFILES)
	$(CXX) -g -o myindex $(LIBS) $(OFILES)
