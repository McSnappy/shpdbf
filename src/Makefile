.PHONY = all debug shptest clean 

CXX = /usr/bin/g++
CXXFLAGS = -O2 -Wall -std=c++11
LDDFLAGS = 

UNAME_S := $(shell uname -s)
ifneq ($(UNAME_S),Darwin)
	CXXFLAGS += 
	LDDFLAGS += 
endif

all: clean shptest 

debug: CXXFLAGS += -g -DDEBUG=1
debug: clean shptest 

shptest:
	$(CXX) $(CXXFLAGS) -L . shptest.cpp dbfutil.cpp shputil.cpp logging.cpp -o shptest $(LDDFLAGS)

clean: 
	rm -f ./shptest
	rm -rf ./shptest.dSYM
