# Create avmlib library from source files 
# and create testavm from test source files and library

# Directories (LIBDIR and OBJDIR will be created if they do not exist)
BINDIR    =bin
LIBDIR    =lib
OBJDIR1    =obj/avmlib
SRCDIR1    =src/avmlib
OBJDIR2   =obj/coretypes
SRCDIR2   =src/coretypes
OBJDIR3   =obj/acorn
SRCDIR3   =src/acorn
TESTDIR   =test
INCLUDE   =-Iinclude

# Important file names
LIBNAME   =avmlib
OBJECTS1   = avm_part.o avm_table.o avm_memory.o avm_gc.o avm_array.o avm_string.o avm_symbol.o avm_value.o avm_thread.o avm_stack.o avm_func.o avm_global.o avm_vm.o
OBJECTS2   = atyp_init.o
OBJECTS3   = acn_gen.o acn_main.o
TESTOBJS  = testavm.o testcapi.o testgen.o
TESTEXE   =testavm

# Transformation options
CC        =gcc
CFLAGS    =-c -Wall -g -O0
LDFLAGS   =-L$(LIBDIR)

# File pathnames derived from above
OBJFILES1   = $(addprefix $(OBJDIR1)/,$(OBJECTS1))
OBJFILES2   = $(addprefix $(OBJDIR2)/,$(OBJECTS2))
OBJFILES3   = $(addprefix $(OBJDIR3)/,$(OBJECTS3))
LIBFILE    = lib$(LIBNAME).a
TESTOBJFILES = $(addprefix $(TESTDIR)/,$(TESTOBJS))

all: $(BINDIR) $(LIBDIR) $(LIBDIR)/$(LIBFILE) $(BINDIR)/$(TESTEXE)

$(BINDIR):
	mkdir $(BINDIR)

$(OBJDIR1):
	mkdir $(OBJDIR1)
$(OBJDIR2):
	mkdir $(OBJDIR2)
$(OBJDIR3):
	mkdir $(OBJDIR3)

$(OBJDIR1)/%.o: $(SRCDIR1)/%.cpp
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDE)

$(OBJDIR2)/%.o: $(SRCDIR2)/%.cpp
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDE)

$(OBJDIR3)/%.o: $(SRCDIR3)/%.cpp
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDE)

$(LIBDIR):
	mkdir $(LIBDIR)

$(LIBDIR)/$(LIBFILE): $(OBJDIR1) $(OBJDIR2) $(OBJDIR3) $(OBJFILES1) $(OBJFILES2) $(OBJFILES3)
	ar rcs $(LIBDIR)/$(LIBFILE) $(OBJFILES1) $(OBJFILES2) $(OBJFILES3)
#	SHARED: gcc -shared -Wl,-soname,$(LIBNAME) -o $(LIBDIR)/$(LIBNAME)  $(OBJFILES)

$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDE)

$(BINDIR)/$(TESTEXE): $(BINDIR) $(LIBDIR) $(LIBDIR)/$(LIBFILE) $(TESTOBJFILES)
	$(CC) $(LDFLAGS) -o $@ $(TESTOBJFILES) -l$(LIBNAME) 

.PHONY: clean

clean:
	rm -rf $(BINDIR)
	rm -rf $(LIBDIR)
	rm -rf $(OBJDIR1)
	rm -rf $(OBJDIR2)
	rm -rf $(OBJDIR3)
	rm -rf $(TESTDIR)/*.o
