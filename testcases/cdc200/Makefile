sources = $(wildcard *.c)
objects = $(sources:.c=.o)
executables = $(objects:.o=)
include_dirs = include/
ldflags =

CFLAGS  += $(addprefix -I,$(include_dirs))
LDFLAGS += $(ldflags)
TEST_PATH = /opt/linux_dd_test/
TEST_NAME = $(shell basename $(CURDIR))

ifeq (,$(DESTDIR))
    DESTDIR = $(shell dirname $(shell dirname $(PWD)))/image
endif

.PHONY: clean all install $(executables)

all: $(executables)

$(executables): $(objects)
	$(CC) $(CFLAGS) -o $@ $@.o $(LDFLAGS)

install:
	install -d $(DESTDIR)$(TEST_PATH)$(TEST_NAME)
	cp -f $(executables) $(DESTDIR)$(TEST_PATH)$(TEST_NAME)

clean:
	$(RM) -rf $(executables) $(objects) $(DESTDIR)
