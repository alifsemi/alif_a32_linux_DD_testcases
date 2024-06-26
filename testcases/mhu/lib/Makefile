# Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
# Use, distribution and modification of this code is permitted under the
# terms stated in the Alif Semiconductor Software License Agreement
#
# You should have received a copy of the Alif Semiconductor Software
# License Agreement with this file. If not, please write to:
# contact@alifsemi.com, or visit: https://alifsemi.com/license
services_objects = $(patsubst %.c,%.o,$(wildcard services_lib*.c))
include_dirs = ../include/
soname_services_lib = libservices.so.0
rname_services_lib = libservices.so.0.0.1
lname_services_lib = libservices.so

CFLAGS  += -g -fPIC $(addprefix -I,$(include_dirs)) -Wno-discarded-qualifiers
LDFLAGS = -L. -lservices
TEST_PATH = /usr/
TEST_NAME = $(shell basename $(CURDIR))

ifeq (,$(DESTDIR))
    DESTDIR = $(shell dirname $(shell dirname $(PWD)))/image
endif

.PHONY: clean all install $(services_lib)

all: $(rname_services_lib) main

main:
	$(CC) $(CFLAGS) $(LDFLAGS) main.c -o main

$(rname_services_lib): $(services_objects)
	$(CC) -shared -Wl,-soname,$(soname_services_lib) -o $(rname_services_lib) $(services_objects)
	ln -sf $(rname_services_lib) $(soname_services_lib)
	ln -sf $(soname_services_lib) $(lname_services_lib)

install:
	install -d $(DESTDIR)$(TEST_PATH)$(TEST_NAME) $(DESTDIR)/opt/linux_dd_test/mhu
	cp -P $(rname_services_lib) $(soname_services_lib) $(lname_services_lib) $(DESTDIR)$(TEST_PATH)$(TEST_NAME)
	install -m 0755 main $(DESTDIR)/opt/linux_dd_test/mhu/main

clean:
	$(RM) -rf $(soname_services_lib) $(rname_services_lib) $(lname_services_lib) main $(services_objects)
