# Create avmlib library from source files 
# and create testavm from test source files and library

# Directories (LIBDIR and OBJDIR will be created if they do not exist)
BINDIR    =bin
LIBDIR    =lib
OBJDIR    =obj
SRCDIR    =src/avmlib
TESTDIR   =test
INCLUDE   =-Iinclude

# Important file names
LIBNAME   =avmlib
OBJECTS   = avm_api.o avm_table.o avm_memory.o avm_array.o avm_string.o avm_symbol.o avm_value.o avm_vm.o
TESTOBJS  = testavm.o
TESTEXE   =testavm

# Transformation options
CC        =gcc
CFLAGS    =-c -Wall
LDFLAGS   =-L$(LIBDIR)

# File pathnames derived from above
OBJFILES   = $(addprefix $(OBJDIR)/,$(OBJECTS))
LIBFILE    = lib$(LIBNAME).a
TESTOBJFILES = $(addprefix $(TESTDIR)/,$(TESTOBJS))

all: $(BINDIR) $(LIBDIR) $(LIBDIR)/$(LIBFILE) $(BINDIR)/$(TESTEXE)

$(BINDIR):
	mkdir $(BINDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDE)

$(LIBDIR):
	mkdir $(LIBDIR)

$(LIBDIR)/$(LIBFILE): $(OBJDIR) $(OBJFILES)
	ar rcs $(LIBDIR)/$(LIBFILE) $(OBJFILES)
#	SHARED: gcc -shared -Wl,-soname,$(LIBNAME) -o $(LIBDIR)/$(LIBNAME)  $(OBJFILES)

$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDE)

$(BINDIR)/$(TESTEXE): $(BINDIR) $(LIBDIR) $(LIBDIR)/$(LIBFILE) $(TESTOBJFILES)
	$(CC) $(LDFLAGS) -o $@ $(TESTOBJFILES) -l$(LIBNAME) 

.PHONY: clean

clean:
	rm -rf $(BINDIR)
	rm -rf $(LIBDIR)
	rm -rf $(OBJDIR)/*.o
	rm -rf $(TESTDIR)/*.o
