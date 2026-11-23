# Small Shell

This project contains two variants of a simple shell written entirely in C:

- `shell.c`: the self-contained simple shell that uses `getline` to read input. It has a known bug in canonical mode where pressing <kbd>Ctrl</kbd> + <kbd>D</kbd> does not EOF the shell on the first try if the user has typed something into the prompt. The terminal simply flushes the incomplete input to `getline`, which keeps waiting for a newline, so a second <kbd>Ctrl</kbd> + <kbd>D</kbd> is needed to trigger the EOF.

- `shell_ln.c`: a [`linenoise`](https://github.com/antirez/linenoise)-based variant of `shell.c` that adds command history and line editing. It "fixes" the <kbd>Ctrl</kbd> + <kbd>D</kbd> bug by behaving exactly how [`readline`](https://savannah.gnu.org/projects/readline) would: if the user has typed something, <kbd>Ctrl</kbd> + <kbd>D</kbd> deletes the character under the cursor. In other words, only an empty prompt can EOF the shell. A true fix involves editing the `linenoise` library itself, which I'm too lazy to do.
