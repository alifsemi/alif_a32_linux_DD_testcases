TESTCASES = $(shell ls -d testcases/*)

.PHONY: all $(TESTCASES)

all: $(TESTCASES)

$(TESTCASES):
	$(MAKE) --directory=$@ $(TARGET)
