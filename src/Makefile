##
## Make senp-lite help program
##
## Targets;
##  help - This printout
##  all (default) - Build the lib
##  clean - Remove built files
##  test - Build test-programs and execute tests
##
## Beside the usual CFLAGS and LDFLAGS some usable variables;
##  O - The output directory. Default /tmp/$USER/bitmaptree
##
## Examples;
##  make -j8
##  make clean
##  make -j8 O=.       # (you *can*, but don't do that!)
##


O ?= /tmp/$(USER)/bitmaptree
LIB ?= $(O)/lib/libbmt.a
DIRS := $(O)/lib/test
LIB_SRC := $(wildcard lib/*.c)
LIB_OBJ := $(LIB_SRC:%.c=$(O)/%.o)

$(O)/%.o : %.c
	$(CC) -c $(CFLAGS) -Wall -I$(NFQLB_DIR)/include -Ilib $< -o $@

.PHONY: all static
all: $(LIB)
$(LIB_OBJ): | $(DIRS)

$(LIB): $(LIB_OBJ)
	@rm -f $(LIB)
	ar rcs $(LIB) $(LIB_OBJ)

.PHONY: test test_progs
$(O)/lib/test/% : lib/test/%.c
	$(CC) $(CFLAGS) -Wall -Ilib $< $(LIB_OBJ) -o $@
TEST_SRC := $(wildcard lib/test/*-test.c)
TEST_PROGS := $(TEST_SRC:%.c=$(O)/%)
$(TEST_PROGS): $(LIB_OBJ)
test_progs: $(TEST_PROGS)
test: $(TEST_PROGS)
	@$(foreach p,$(TEST_PROGS),echo $(p);$(p);)

$(DIRS):
	@mkdir -p $(DIRS)

.PHONY: clean
clean:
	rm -rf $(LIB) $(LIB_OBJ) $(TEST_PROGS)

.PHONY: help
help:
	@grep '^##' $(lastword $(MAKEFILE_LIST)) | cut -c3-
	@echo "Binary:"
	@echo "  $(X)"
