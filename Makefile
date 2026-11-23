CC     = cc
BUILD  = build
STRICT = -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic

# linenoise needs strcasecmp (_DEFAULT_SOURCE) and uses variadic macros
# that -pedantic -Werror rejects
LNFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Wall -Wextra

.PHONY: all clean
all: $(BUILD)/shell $(BUILD)/shell_ln

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/shell: shell.c | $(BUILD)
	$(CC) $(STRICT) shell.c -o $@

$(BUILD)/shell_ln.o: shell_ln.c linenoise/linenoise.h | $(BUILD)
	$(CC) $(STRICT) -c shell_ln.c -o $@

$(BUILD)/linenoise.o: linenoise/linenoise.c linenoise/linenoise.h | $(BUILD)
	$(CC) $(LNFLAGS) -c linenoise/linenoise.c -o $@

$(BUILD)/shell_ln: $(BUILD)/shell_ln.o $(BUILD)/linenoise.o
	$(CC) $^ -o $@

clean:
	rm -rf $(BUILD)
