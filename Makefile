CPPFLAGS = $(shell pkg-config --cflags gmime-3.0)
#CPPFLAGS += $(shell xapian-config --cxxflags)
LIBS = $(shell pkg-config --libs gmime-3.0)
LIBS += $(shell xapian-config --cxxflags --libs)
LIBS += -lgcrypt
CXXFILES = xapianglue myindex tokenizer util
OFILES = $(CFILES:=.o) $(CXXFILES:=.o)
CXXFLAGS = -Wall -W -O2 -g

all: myindex

clean:
	-rm -f myindex *.o *.pyc

myindex: $(OFILES)
	$(CXX) -g -o myindex $(OFILES) $(LIBS) 
