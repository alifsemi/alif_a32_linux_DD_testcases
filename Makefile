TESTCASES = $(shell ls -d testcases/*)

.PHONY: all $(TESTCASES)

all: $(TESTCASES)

$(TESTCASES):
	$(MAKE) --directory=$@ $(TARGET)

clean:
	for iter in $(TESTCASES) ; do \
	$(MAKE) --directory=$$iter clean ; \
	done
