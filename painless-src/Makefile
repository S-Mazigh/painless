SRCS = $(shell find . -name "*.cpp")

OBJS = $(addsuffix .o, $(basename $(SRCS)))

EXEC = painless


#/kissat_mab/build
#../my_kissat_mab/build
KISSAT_BUILD := ../kissat_mab/build

KISSAT_DFLAGS := $(shell cat $(KISSAT_BUILD)/makefile | grep -oP "CFLAGS=(.+)" | grep -o '\-D[^ ]*')

KISSAT_DEBUG := $(shell cat $(KISSAT_BUILD)/makefile | grep -oP "CFLAGS=(.+)" | grep -o '\-O[^ ]*')
KISSAT_DEBUG += $(shell cat $(KISSAT_BUILD)/makefile | grep -oP "CFLAGS=(.+)" | grep -o '\-g[^ ]*')


CXX = g++#mpic++

#        -L../openMPI/lib -lmpi
LIBS = -lkissat -L$(KISSAT_BUILD) \
       -lmapleCOMSPS -L../mapleCOMSPS/build/release/lib/ \
       -l:libm4ri.a -L../mapleCOMSPS/m4ri-20140914/.libs \
       -lpthread -lz -lm $(shell mpic++ --showme:link)

# -I../openMPI/include
CXXFLAGS = -I. \
           -I.. \
           -I../mapleCOMSPS \
           $(shell mpic++ --showme:compile)\
           -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS $(KISSAT_DFLAGS) \
           -fopenmp -std=c++17 $(KISSAT_DEBUG) # -fsanitize=address -DQUIET #


$(info "KISSAT_DFLAGS = $(KISSAT_DFLAGS), KISSAT_DEPENDENCIES = $(KISSAT_DEPENDENCIES)" KISSAT_DEBUG = $(KISSAT_DEBUG))

$(EXEC): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXEC)
