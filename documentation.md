(aka Shreya needs a place to put her excessive commenting/notes)

## enableRawMode
```C 
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
```
This statement turns off the appropriate flags to enable raw mode. raw is the copy of orig_termios, the termios structure that represents the original characteristics of the terminal. One of the fields of the termios struct is c_lflag, which basically encapsulates all of the miscellaneous flags. These flags include:
* ISIG - controls responses to special characters like CTRL+C and CTRL+Z. By turning off this flag, we can disable the standard responses to these characters.
* ECHO - outputs the characters you're typing to the terminal. By turning this off, we can take control of what is actually printed to the terminal. *Fun fact! sudo also turns ECHO off!*
* ISCANON - controls canonical mode. By turning off this flag, we can turn off canonical mode.
* IEXTEN - controls responses to characters like CTRL+V and CTRL+0. By turning off this flag, we can disable the standard responses to these key combinations.
The actual mechanics of turning off these flags are just bit-wise operations. We first use the bitwise-OR operator (|) to combine all of these flags. We then use the bitwise-NOT operator (~) to turn off these flags. Finally, we use the bitwise-AND operator to set these changes in effect in the c_lflag bitfield, by flipping the bits corresponding to the flags that we changed and preserving the other bits.

```
  C raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
```
Like the above statement, this statement turns off more flags that need to be disabled to enable raw mode. This statement is specifically modifying flags that control input modes (which is what the c_iflag field represents). These are the flags we modify:
* BRKINT - This flag causes the input and output queues to be flushed when a BREAK is received and a SIGINT signal is sent to the program. We turn this off so that we can control when this signal is sent to the program.
* INPCK - This flag enables parity checking.
* ISTRIP - This flag causes the 8th bit of each input byte to be stripped off/set to 0.
* IXON - This flag controls the responses to CTRL+S and CTRL+0, so we disable the automatic responses.
* ICRNL - This flag automatically translates carriage returns to new lines. By turning this flag, we also fix the CTRL+M command as now we can read CTRL+M as a regular byte.

```
  C raw.c_oflag &= ~(OPOST);
```
The above statement turns off the output flags (whch is what the c_oflag bitfield represents). Specifically, we turned off the OPOST flag, which controls all output processing.

```
  C raw.c_cflag |= (CS8);
```
CS8 is a bit mask with multiple bits, which is set using the bitwise-OR (|) operator. This sets character size (CS) to be 8 bits per byte.

```
  C raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
```
The c_cc field in the termios struct stands for "control characters", which is an array of bytes that controls various terminal settings.
* VMIN - the minimum number of bytes of input needed before read() can be returned. By setting it to 0, read() reurns as soon as there is any input to be read.
* VTIME - the maximum amount of time to wait before read() returns, measured in tenths of a second. We set the time out to 1/10th of a second, or 100ms. If read() times out, rather than returning the number of bytes read, it returns 0 (which is an appropriate error code to indicate timeout).

```
  C tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
```
* Resets the pointer to the termios object (which represents
the characteristics of the terminal) to raw, the termios object we created
which has the updated characteristics of raw mode.
* The TCSAFLUSH parameter means that these changes will only occur after all
the output from STDIN_FILENO has been transmitted appropriately and all input
that has been received but not read has been discarded (hence FLUSH).

## editorReadKey and editorProcessKeypress

```
  C read(STDIN_FILENO, &c, 1);
```
Read user input one byte at a time into character c.
Reads from standard input but considers standard input like a file, so
that we can use the read() function to determine if there are still any
unread bytes in the file.

```
  C #define CTRL_KEY(k) ((k) & 0x1f)
```
The CTRL_KEY macro is constructed through a bitwise-AND operation. We bitwise-AND whatever character is received with 0x1f, which is b00011111. This sets the upper bits of the character to be 0 and preserves the other bits, effectively mirroring the effect of the CTRL key.

## editorRefreshScreen

```
  C write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
```

@TODO: Kristin document this!

## getWindowSize

```
C struct winsize ws;
if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
  return -1;
} else {
  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
}
```
TIOCGWINSZ stands for Terminal IOCtl (IOCtl = Input/Output/Control) Get Window Size. If the ioctl command is successful, then the winsize struct is updated with the number of columns and rows of the editor. Otherwise, the function returns a -1.


## abAppend and abFree

```
C void abAppend(struct abuf *ab, const char *s, int len) {
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
