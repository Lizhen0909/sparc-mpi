# linux = Linux desktop machine, g++, MPICH

SHELL = /bin/sh

# ---------------------------------------------------------------------
# compiler/linker settings
# specify flags and libraries needed for your compiler and MPI

CC =		mpic++
CCFLAGS =	-O -DMPICH_IGNORE_CXX_SEEK
SHFLAGS =	-fPIC
DEPFLAGS =	-M

ARCHIVE =	ar
ARFLAGS =	-rvs
SHLIBFLAGS =	-shared
SHLIB =         -lmpich -lmpl -lpthread

# ---------------------------------------------------------------------
# build rules and dependencies
# no need to edit this section

SOURCES=$(wildcard *.cpp)
OBJECTS=$(patsubst %.cpp, %.o, $(SOURCES))
OBJ=$(OBJECTS)
EXE=libmrmpi.a
ifndef INSTALL_DIR
   INSTALL_DIR=/usr/local
endif

# Library targets

all: lib

lib:	$(OBJ)
	$(ARCHIVE) $(ARFLAGS) $(EXE) $(OBJ)

shlib:	$(OBJ)
	$(CC) $(CCFLAGS) $(SHFLAGS) $(SHLIBFLAGS) -o $(EXE) $(OBJ) $(SHLIB)

# Compilation rules

%.o:%.cpp
	$(CC) $(CCFLAGS) $(SHFLAGS) -c $<

%.d:%.cpp
	$(CC) $(CCFLAGS) $(DEPFLAGS) $< > $@

# Individual dependencies

DEPENDS = $(OBJ:.o=.d)
include $(DEPENDS)

install: lib
	mkdir -p $(INSTALL_DIR)/include/mrmpi && cp *.h $(INSTALL_DIR)/include/mrmpi && mkdir -p $(INSTALL_DIR)/lib/ && cp libmrmpi.a $(INSTALL_DIR)/lib/

clean:
	rm -fr $(OBJECTS) $(EXE)

