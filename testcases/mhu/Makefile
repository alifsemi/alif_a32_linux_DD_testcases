# Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
# Use, distribution and modification of this code is permitted under the
# terms stated in the Alif Semiconductor Software License Agreement
#
# You should have received a copy of the Alif Semiconductor Software
# License Agreement with this file. If not, please write to:
# contact@alifsemi.com, or visit: https://alifsemi.com/license
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
