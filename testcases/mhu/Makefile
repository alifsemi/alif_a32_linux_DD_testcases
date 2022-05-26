sources = $(wildcard *.c)
objects = $(sources:.c=.o)
executables = $(objects:.o=)
include_dirs = include/
#ldflags = -lpthread

CFLAGS  += -g -fPIC $(addprefix -I,$(include_dirs))
LDFLAGS += $(ldflags)
TEST_PATH = /opt/linux_dd_test/
TEST_NAME = $(shell basename $(CURDIR))

ifeq (,$(DESTDIR))
    DESTDIR = $(shell dirname $(shell dirname $(PWD)))/image
endif

.PHONY: clean all install make_lib $(executables)

all: $(executables) make_lib

$(executables): $(objects)
	$(CC) $(CFLAGS) -o $@ $@.o $(LDFLAGS)

make_lib:
	make -C lib

install:
	install -d $(DESTDIR)$(TEST_PATH)$(TEST_NAME)
	cp -f $(executables) $(DESTDIR)$(TEST_PATH)$(TEST_NAME)
	make -C lib install

clean:
	$(RM) -rf $(executables) $(objects) $(DESTDIR)
	make -C lib clean
