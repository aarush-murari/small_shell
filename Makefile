all:
	cc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic shell.c -o shell
