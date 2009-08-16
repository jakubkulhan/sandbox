# INTELIGENT MAKEFILE
# programs, etc.
CXX=g++
CC=gcc
LD=g++
RM=rm -rf

# flags
include $(shell find . -name "flags.mk")

LDFLAGS?=
CFLAGS?=-Wall -pedantic -O2
CXXFLAGS?=$(CFLAGS)
VERBOSE?=NO
EXCLUDE_DIRS?=.git

# verbosity
ifeq ($(VERBOSE),NO)
	line_start=@
	MAKE+=-s
endif

# basic definitions
pwd=$(shell pwd)
TARGET=$(notdir $(pwd))
OBJECTS=$(patsubst %.cpp, %.o, $(wildcard *.cpp)) $(patsubst %.c, %.o, $(wildcard *.c))
SUBDIRS=$(shell find . -maxdepth 1 -and -type d -not -name "." $(foreach exc, $(EXCLUDE_DIRS), -not -name "$(exc)") | sed -e 's@^./@@')
MAKEFILE?=$(pwd)/Makefile
MK=$(MAKE) -C $(DIR) -f $(MAKEFILE) MAKEFILE=$(MAKEFILE) startdir=$(startdir)/

# startdir / currentdir
startdir ?= $(pwd)
currentdir=$(shell echo $(pwd) | sed 's@^$(startdir)@@')
ifeq ($(strip $(currentdir)),)
	currentdir=.
endif

# inteligent first target
all_targets=
ifneq ($(strip $(shell find . -maxdepth 1 -name "main.cpp" -or -name "main.c")),)
	all_targets+=$(TARGET)
endif

ifneq ($(strip $(SUBDIRS)),)
	all_targets+=subdirs
endif
all: $(all_targets)
.PHONY: all

# other targets
subdirs:
	@$(foreach DIR, $(SUBDIRS), $(MK);)
.PHONY: subdirs

$(TARGET): $(OBJECTS)
ifeq ($(strip $(currentdir)),.)
	@echo "EXE $(TARGET)"
else
	@echo "EXE $(currentdir)/$(TARGET)"
endif
	$(line_start)$(LD) $(LDFLAGS) $(OBJECTS) -o $(TARGET)

%.o: %.cpp
ifeq ($(strip $(currentdir)),.)
	@echo "CXX $<"
else
	@echo "CXX $(currentdir)/$<"
endif
	$(line_start)$(CXX) $(CXXFLAGS) $< -c -o $@

%.o: %.c
ifeq ($(strip $(currentdir)),.)
	@echo "CC  $<"
else
	@echo "CC  $(currentdir)/$<"
endif
	$(line_start)$(CC) $(CFLAGS) $< -c -o $@

clean:
	@echo "RM  $(foreach obj, $(OBJECTS), $(currentdir)/$(obj)) $(currentdir)/$(TARGET)"
	$(line_start)$(RM) $(OBJECTS) $(TARGET) *~
	@$(foreach DIR, $(SUBDIRS), $(MK) clean;)
.PHONY: clean
