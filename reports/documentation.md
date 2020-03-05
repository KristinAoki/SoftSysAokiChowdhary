(aka Shreya needs a place to put her excessive commenting/notes)

## enableRawMode
```
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
```
This statement turns off the appropriate flags to enable raw mode. `raw` is the copy of `orig_termios`, the `termios` structure that represents the original characteristics of the terminal. One of the fields of the `termios` struct is `c_lflag`, which basically encapsulates all of the miscellaneous flags. These flags include:
* `ISIG` - controls responses to special characters like CTRL+C and CTRL+Z. By turning off this flag, we can disable the standard responses to these characters.
* `ECHO` - outputs the characters you're typing to the terminal. By turning this off, we can take control of what is actually printed to the terminal. *Fun fact! sudo also turns ECHO off!*
* `ISCANON` - controls canonical mode. By turning off this flag, we can turn off canonical mode.
* `IEXTEN` - controls responses to characters like CTRL+V and CTRL+0. By turning off this flag, we can disable the standard responses to these key combinations.
The actual mechanics of turning off these flags are just bit-wise operations. We first use the bitwise-OR operator (|) to combine all of these flags. We then use the bitwise-NOT operator (~) to turn off these flags. Finally, we use the bitwise-AND operator to set these changes in effect in the `c_lflag` bitfield, by flipping the bits corresponding to the flags that we changed and preserving the other bits.

```
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
```
Like the above statement, this statement turns off more flags that need to be disabled to enable raw mode. This statement is specifically modifying flags that control input modes (which is what the `c_iflag` field represents). These are the flags we modify:
* `BRKINT` - This flag causes the input and output queues to be flushed when a BREAK is received and a SIGINT signal is sent to the program. We turn this off so that we can control when this signal is sent to the program.
* `INPCK` - This flag enables parity checking.
* `ISTRIP` - This flag causes the 8th bit of each input byte to be stripped off/set to 0.
* `IXON` - This flag controls the responses to CTRL+S and CTRL+0, so we disable the automatic responses.
* `ICRNL` - This flag automatically translates carriage returns to new lines. By turning this flag, we also fix the CTRL+M command as now we can read CTRL+M as a regular byte.

```
raw.c_oflag &= ~(OPOST);
```
The above statement turns off the output flags (whch is what the `c_oflag` bitfield represents). Specifically, we turned off the `OPOST` flag, which controls all output processing.

```
  raw.c_cflag |= (CS8);
```
`CS8` is a bit mask with multiple bits, which is set using the bitwise-OR (|) operator. This sets character size (CS) to be 8 bits per byte.

```
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
```
The `c_cc` field in the termios struct stands for "control characters", which is an array of bytes that controls various terminal settings.
* `VMIN` - the minimum number of bytes of input needed before `read()` can be returned. By setting it to 0, `read()` returns as soon as there is any input to be read.
* `VTIME` - the maximum amount of time to wait before `read()` returns, measured in tenths of a second. We set the time out to 1/10th of a second, or 100ms. If `read()` times out, rather than returning the number of bytes read, it returns 0 (which is an appropriate error code to indicate timeout).

```
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
```
* Resets the pointer to the `termios` object (which represents
the characteristics of the terminal) to `raw`, the `termios` object we created
which has the updated characteristics of raw mode.
* The `TCSAFLUSH` parameter means that these changes will only occur after all
the output from STDIN_FILENO has been transmitted appropriately and all input
that has been received but not read has been discarded (hence FLUSH).

## editorReadKey and editorProcessKeypress

```
  read(STDIN_FILENO, &c, 1);
```
Read user input one byte at a time into character `c`.
Reads from standard input but considers standard input like a file, so
that we can use the `read()` function to determine if there are still any
unread bytes in the file.

```
  #define CTRL_KEY(k) ((k) & 0x1f)
```
The `CTRL_KEY` macro is constructed through a bitwise-AND operation. We bitwise-AND whatever character is received with `0x1f`, which is `b00011111`. This sets the upper bits of the character to be 0 and preserves the other bits, effectively mirroring the effect of the CTRL key.

## editorRefreshScreen

```
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
```

@TODO: Kristin document this!

```
char buf[32];
snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy +1, E.cx + 1 );
abAppend(&ab, buf, strlen(buf));
```
We convert the position of the cursor from 0-indexed values to 1-indexed
values, and send the new positions to the cursor position command (the H command).

## getWindowSize

```
struct winsize ws;
if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
  return -1;
} else {
  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
}
```
`TIOCGWINSZ` stands for Terminal IOCtl (IOCtl = Input/Output/Control) Get Window Size. If the `ioctl` command is successful, then the `winsize` struct is updated with the number of columns and rows of the editor. Otherwise, the function returns a -1.


## abAppend and abFree

```
void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}
```

We define `abuf`, a struct that consists of a pointer to our buffer in memory and a length. When we want to append a string to `abuf`, first we have to allocate enough memory to hold the string. The `realloc` function takes a pointer to the current string and the new size, which the length of the string currently in `abuf` plus the length of the new string we are adding in. `realloc` will deallocate the memory allocated to the current string in `abuf` and return a pointer to a new object that has the specified size. We copy the new string s to the end of the current data in the buffer, and then update the pointers and length of the `abuf`. Afterwards,`abFree` deallocates the memory that was allocated by `abuf`.

# editorDrawRows

```
abAppend(ab, "\x1b[K", 3);
```
The escape sequence could accept additional commands to set exactly how the line would be erased. A 2 would erase the whole line, while 1 would erase the part of the line to the left of the cursor, and a 0 would erase the part of the line to the right of the cursor. We wanted a normal line erase, so we wanted setting 0, which is also the default setting, which is why we just used `<esc>[K]`.

```
int welcomelen = snprintf(welcome, sizeof(welcome), "Nucleus Editor -- version %s", NUCLEUS_VERSION);
if (welcomelen > E.screenCols) {
  welcomelen = E.screenCols;
}
```
`snprintf` is a special version of `printf` that comes from `<stdio.h>`. It formats and stores a series of characters and values in the array buffer. The first argument is the buffer, the second is the size or the maximum number of characters, and the last two arguments are the format strings. Here, we use `snprintf` to fill the `welcome` string buffer with the welcome message. In the process, we also get the length of the string, which is helpful in displaying it later. THe condition that follows truncates the length of the string in case the terminal is too small to fit the welcome message.

## General notes about escape sequences
* Escape sequences are sequences of characters that does not represent itself when used inside a character or string literal, but is translated into another character or a sequence of characters that is difficult/impossible to represent directly. (from the Wikipedia Article)
* `\n` is an escape sequence, because it does not stand for a backslash followed by the letter n. Instead, the backlash causes can "escape" from the normal way characters are interpreted, and leads the compiler to expect another character to complete the escape sequence.
* The main escape sequence that we're using is `\xhh`, which gives you the byte whose numerical value is given by `hh` interpreted as a hexadecimal number. So for example, `\x12` represents a single character with value `0x12` or 18.
* In this project, we use VT100 escape sequences, which are supported widely by most modern terminal emulators. The structure of a VT100 escape sequence is as follows: an escape sequence (like `\x`), a `[`, a paramter string (often a combination of numbers and letters), and the final character. Some of the escape sequences we utilized were:
  * `<esc>[2J`, or the erase in display command - to clear the screen. The parameter string for this command is a number and the letter J. The number is either 0, 1, or 2, and controls where the cursor starts when clearing the sceen. 0 would clear from the cursor to the end of the screen (and is the default command), 1 would clear the screen up to where the cursor is, and 2 would clear the entire screen. We use this escape sequence in the `editorRefreshScreen()` function, so it makes sense to set the numerical parameter to 2.
  * `<esc>[K`, or the erase in line command - to erase one line at a time. Like the J command, this command's parameter string is also a number and the letter K. The numerical parameter also functions similarly: 2 would erase the whole line, 1 would erase the part of the line to the left of the cursor, and 0 erases the part of the line to the right of the cursor (which is the type of deletion we want). Because 0 is the default argument, we can leave out the argument: `<esc>[K`.
  * `<esc>[H`, or the cursor position command. The parameter string for this command is two numbers separated by a ; and H. The first numerical parameter specifies the line position (the row number) and the second specifies the column position (the column number). The default arguments are 1, so we can set the cursor to the first row and first column by leaving them out.
