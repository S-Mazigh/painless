SRCS := $(shell find . -name "*.cpp" -not -path "./.ignores/*")

OBJS = $(addsuffix .o, $(basename $(SRCS)))

EXEC = painless

YALSAT_BUILD := ../solvers/yalsat

KISSAT_BUILD := ../solvers/kissat_mab/build

KISSATGASPI_BUILD := ../solvers/GASPIKISSAT/build

# KISSAT_DFLAGS := $(shell cat $(KISSAT_BUILD)/makefile | grep -oP "CFLAGS=(.+)" | grep -o '\-D[^ ]*')

CXX := g++#mpic++

STATIC_FLAGS := #-static-libstdc++ -static-libgcc


LIBS := -l:libyals.a -L$(YALSAT_BUILD) \
       -l:libkissat.a -L$(KISSAT_BUILD) \
       -l:libgkissat.a -L$(KISSATGASPI_BUILD) \
       -l:libmapleCOMSPS.a -L../solvers/mapleCOMSPS/build/release/lib/ \
       -l:libm4ri.a -L../libs/m4ri-20200125/.libs \
       -lpthread -lz -lm $(shell mpic++ --showme:link)

CXXFLAGS := -I. \
           -I../solvers \
           -I../libs/eigen-3.4.0 \
           -I../libs/m4ri-20200125 \
           $(shell mpic++ --showme:compile)\
           -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS \
           -fopenmp $(STATIC_FLAGS) -std=c++17 -O3 -DNDEBUG#-O0 -g3 #-O3 -DNDEBUG#-fsanitize=address#-fsanitize=thread #

$(EXEC): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXEC)
