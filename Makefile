# Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
# Use, distribution and modification of this code is permitted under the
# terms stated in the Alif Semiconductor Software License Agreement
#
# You should have received a copy of the Alif Semiconductor Software
# License Agreement with this file. If not, please write to:
# contact@alifsemi.com, or visit: https://alifsemi.com/license

TESTCASES = $(shell ls -d testcases/*)

.PHONY: all $(TESTCASES)

all: $(TESTCASES)

$(TESTCASES):
	$(MAKE) --directory=$@ $(TARGET)

clean:
	for iter in $(TESTCASES) ; do \
	$(MAKE) --directory=$$iter clean ; \
	done
