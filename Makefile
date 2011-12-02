CC=gcc
CFLAGS=-O2 -s -Wall -I./
LDFLAGS=-lpthread

PRIVATE_SOURCES := $(shell find . -iname *.c)
SHARED_SOURCES := $(shell ls -1 ../shared/*.c)

SOURCES := $(PRIVATE_SOURCES) $(SHARED_SOURCES)

OBJDIR	:= ../build-$(shell uname -m)

OBJECTS := $(SOURCES:%.c=$(OBJDIR)/%.o)
OBJDIRS := $(sort $(dir $(OBJECTS)))
DEPS 	:= $(OBJECTS:.o=.d)
EXECUTABLE=$(OBJDIR)/source

SOURCE_DIR := source

PRJ := $@

source:
	override $(PRJ) = $($(SOURCE_DIR))

all: source $(OBJDIRS) $(OBJDIRS) $(DEPS) $(CONFHEAD) $(EXECUTABLE)
	@echo Done. Run $(EXECUTABLE) and have fun.

$(VARS):

$(OBJDIR):
	@echo "build to "$(OBJDIR)/
	@test -d $(OBJDIR) || mkdir $(OBJDIR)

$(OBJDIRS): $(OBJDIR)
	@test -d $@ || mkdir $@

$(EXECUTABLE): $(OBJECTS)
	@echo LD     $@
	@$(CC) $(LDFLAGS) $(OBJECTS) -o $@

$(OBJDIR)/%.o: %.c
	@echo CC     $@
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.d: %.c $(OBJDIRS)
	@echo DEP    $@
	@$(CC) -MM -MG $(CFLAGS) $< | sed -e 's,^\([^:]*\)\.o[ ]*:,$(@D)/\1.o $(@D)/\1.d:,' >$@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	@echo Cleaning..
	@rm -f $(EXECUTABLE)
	@rm -f $(OBJECTS)
	@rm -f $(DEPS)

