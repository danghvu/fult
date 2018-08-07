CC = icc
CFLAGS += -g3 -ggdb -O3 -Wall -Wextra -fPIC  -ftls-model=initial-exec
LDFLAGS += -shared -Lstatic

AR ?= ar
RANLIB ?= ranlib
PREFIX ?= /usr

SRCDIR = ./src
OBJDIR ?= ./obj

JFCONTEXT = $(SRCDIR)/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = $(SRCDIR)/make_x86_64_sysv_elf_gas.S
OBJS = mfcontext.o jfcontext.o fult.o thread_pool.o lcrq.o

LIBOBJ = $(addprefix $(OBJDIR)/, $(OBJS))

ARCHIVE = libfult.a
LIBRARY = libfult.so

all: $(ARCHIVE) $(LIBRARY)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $^ -o $@ -I./include -I/opt/apps/valgrind/3.12.0/include -DUSE_VALGRIND

$(OBJDIR)/jfcontext.o: $(JFCONTEXT)
	$(CC) -O3 -c $(JFCONTEXT) -o $(OBJDIR)/jfcontext.o

$(OBJDIR)/mfcontext.o: $(MFCONTEXT)
	$(CC) -O3 -c $(MFCONTEXT) -o $(OBJDIR)/mfcontext.o

$(LIBRARY): $(LIBOBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(ARCHIVE): $(LIBOBJ)
	@rm -f $(ARCHIVE)
	$(AR) q $(ARCHIVE) $(LIBOBJ)
	$(RANLIB) $(ARCHIVE)

clean:
	rm -rf $(ARCHIVE) $(LIBRARY) $(OBJDIR)/*.o
