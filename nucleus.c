/*** includes ***/
// Added in case compiler complains about the function get_line
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}
#define NUCLEUS_VERSION "0.0.1"
#define NUCLEUS_TAB_STOP 8
#define NUCLEUS_QUIT_TIMES 3

// enum to define constants for the arrow keys, etc
enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DELETE_KEY
};

/*** data ***/
// Structure to represent a row in the editor
/* struct fields:
- int size - integer length of string
- char *chars - string of text to be put in this row
- int rsize - size of contents of render
- char *render - characters that should actually be displayed
*/
typedef struct erow {
  int size;
  char *chars;
  int rsize;
  char *render;
} Erow;

// Structure to represent the editor state
/* struct fields:
- struct termios orig_termios - termios object that represents the terminal
- int screenRows - the number of rows on the screen
- int screenCols - the number of columns on the screen
- int cx, cy - the x & y coordinates of the cursor
- int rx - index into the render field of an row, so cursor can be moved
to the current position
- int numrows - the number of rows to text
- Erow *erow - an array of rows
- int rowoff - the offset variable, which keeps track of the row
- int coloff - the offset variable, keeps track of the column
- char *filename - string storing filename
- char statusmsg[100] - buffer for the status message string
- time_t statusmsg_time - time since status message was updated
- int dirty - number of changes that have been made
the user is currently scrolled to
*/
typedef struct editorConfig {
  // Structure to represent the terminal
  struct termios orig_termios;
  int screenRows;
  int screenCols;
  int cx, cy;
  int rx;
  int numrows;
  Erow *row;
  int rowoff;
  int coloff;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  int dirty;
} Editor;

Editor E;

// Structure to represent text that has been added
/* struct fields:
- char *b - string in buffer
- int len - the length of the string in buffer
*/
typedef struct abuf {
  char *b;
  int len;
} Abuf;

/*** prototypes ***/

// Able to call function before it is defined.
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt);

/*** terminal ***/
void die(const char *s) {
  // Clear the screen when the program exits
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  // prints descriptive error message for the gloabl errno variable,
  // that is set when something fails
  perror(s);
  exit(1);
}

/*
Adds a string to the buffer.
*/
void abAppend(Abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}
/*
Restores the attributes of the terminal to the original state.
*/
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

/*
Changes the attributes of the terminal to enable raw mode.
*/
void enableRawMode() {
  // Gets the attributes of the current terminal
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  // Makes sure to disable raw mode and return terminal to original state on exit
  atexit(disableRawMode);

  // Essentially copies the attributes of the current terminal
  // (before enabling raw mode)
  struct termios raw = E.orig_termios;

  /* Turns off the appropriate flags by modifying the structure.*/
  // Turns off appropriate miscellaneous flags
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  // Turns off appropriate input mode flags
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  // Turn off all output processing
  raw.c_oflag &= ~(OPOST);
  //Making sure character size is 8 bits per byte.
  raw.c_cflag |= (CS8);

  // Set timeout
  raw.c_cc[VMIN] = 0;
  //Time counted in tenths of a second.
  raw.c_cc[VTIME] = 1;

  // Updates the terminal characteristics
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) die("tcsetattr");
}

/*
Waits for one key press and returns it.
*/
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  // Checks for start of an escape character
  if (c == '\x1b') {
    char seq[3];

    /* Read the next two bytes into the seq buffer, and return the Escape key
    if either of these reads times out (assuming that the user pressed the
  Escape key). */
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    // Check for arrow key escape sequence
    if (seq[0] == '[') {
      if (seq[0] >= '0' && seq[1] <= '9') {
        // Attempt to read another byte and if there's nothing,
        // assume escape key
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

        // A tilde indicates one of the following keys
        // HOME_KEY and END_KEY are handled multiple times because there
        // are multiple possible escape sequences that map to these keys,
        // depending on the OS or terminal emulator.
        if (seq[2] =='~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DELETE_KEY;
            case '4': return END_KEY;
            // PAGE_UP and PAGE_DOWN = <esc>[5~, <esc>[6~
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else{
        switch (seq[1]) {
          /* the cases are the values used to code for arrow key values
            A = the up arrow
            B = the down arrow
            C = the right arrow
            D = the left arrow
          */
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else{
  return c;
  }
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

int editorRowCxToRx(Erow *row, int cx) {
  int rx = 0;
  for (int i = 0; i < cx; i++) {
    // Determine if a tab was used
    if (row->chars[i] == '\t'){
      // If there is a tab, determine how many columns we
      // are to the left of the next tab stop
      rx += (NUCLEUS_TAB_STOP -1) - (rx % NUCLEUS_TAB_STOP);
    }
    rx++;
  }
  return rx;
}

void editorUpdateRow(Erow *row) {
  int i;

  // Calculate number of tabs
  int tabs = 0;
  for (i = 0; i < row->size; i++) {
    if (row->chars[i] == '\t') tabs++;
  }

  // Clear and allocate space for new render
  free(row->render);
  row->render = malloc(row->size + tabs*(NUCLEUS_TAB_STOP -1) + 1);

  int idx = 0;
  for (i = 0; i < row->size; i++) {
    // Add tabs if user hit tab
    if (row->chars[i] == '\t') {
      row->render[idx++] = ' ';
      // Add the appropriate number of spaces
      while (idx % NUCLEUS_TAB_STOP != 0 ) {
        row->render[idx++] = ' ';
      }
      // Copy over character from string if not tab
    } else {
        row->render[idx++] = row->chars[i];
    }
  }
  row->render[idx] = '\0';

  /*After the for loop, idx contains the number of characters we copied into
  row->render, so we assign it to row->rsize. */
  row->rsize = idx;
}

/*
Adds a row of given text s to the editor, by allocating space for a new
row and then copying the given string s to a new row at the end of the array
of rows.
*/
void editorInsertRow(int idx, char *s, size_t len) {
  if (idx < 0 || idx > E.numrows) return;

  // Allocate space for a new row
  E.row = realloc(E.row, sizeof(Erow)*(E.numrows + 1));
  memmove(&E.row[idx+1], &E.row[idx], sizeof(Erow)*(E.numrows-idx));

  // Add last row to arrow of rows
  E.row[idx].size = len;
  E.row[idx].chars = malloc(len+1);
  // Put row contents in new row
  memcpy(E.row[idx].chars, s, len);
  E.row[idx].chars[len] = '\0';
  E.row[idx].rsize = 0;
  E.row[idx].render = NULL;

  editorUpdateRow(&E.row[idx]);

  // Update number of rows in editor
  E.numrows++;
  // Indicate that changes have been made
  E.dirty++;
}

/*
Free the memory occupied by a specific row.
*/
void editorFreeRow(Erow *row) {
  free(row->render);
  free(row->chars);
}

/*
Delete row at a specific index
*/
void editorDelRow(int idx) {
  // Check for valid row index
  if (idx < 0 || idx >= E.numrows) return;
  // Free row
  editorFreeRow(&E.row[idx]);
  // Overwrite row with rows that come after it
  memmove(&E.row[idx], &E.row[idx+1], sizeof(Erow) * (E.numrows - idx - 1));
  // Update number of rows
  E.numrows--;
  // Indicate change
  E.dirty++;
}

/*
Insert a character into a given row in the editor.
*/
void editorRowInsertChar(Erow *row, int idx, int c) {
  // Check for valid position to enter character; if not, put character at end of string
  if (idx < 0 || idx > row->size){
    idx = row->size;
  }

  // Reallocate space for the new character (and a null terminator)
  row->chars = realloc(row->chars, row->size + 2);

  // Move memory block containing old string over
  memmove(&row->chars[idx + 1], &row->chars[idx], row->size - idx + 1);
  // Add character to buffer string for row
  row->size++;
  row->chars[idx] = c;
  // Update the row in the editor
  editorUpdateRow(row);
  // Indicate that a change has been made
  E.dirty++;
}

void editorRowAppendString(Erow *row, char *s, size_t len) {
  // Reallocate space for the row and the new string
  row->chars = realloc(row->chars, row->size + len + 1);
  // Copy string to end of row
  memcpy(&row->chars[row->size], s, len);
  // Update length of row & null terminate row
  row->size += len;
  row->chars[row->size] = '\0';
  // Update row
  editorUpdateRow(row);
  // Indicate change
  E.dirty++;
}

/*
Delete character from given row in the editor.
*/
void editorRowDelChar(Erow *row, int idx) {
  if (idx < 0 || idx >= row->size) return;
  // Overwrite deleted character with characters that come after it
  memmove(&row->chars[idx], &row->chars[idx + 1], row->size - idx);
  // Update row size and row
  row->size--;
  editorUpdateRow(row);
  // Indicate new change
  E.dirty++;
}

/*** editor operatios ***/

// Insert a character into the position that the
// character is at (at the editor level)
void editorInsertChar(int c) {
  // Add a row to botto of file if you are on the last row
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewLine() {
  // If at beginning of line, insert new row before line we're on
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    // Get current row
    Erow *row = &E.row[E.cy];
    // Insert row below with the correct contents
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    // Reassign row pointer and truncate contents
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  // Move cursor position to beginning of new line
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  // If cursor at end of file, nothing to do
  if (E.cy == E.numrows) return;
  // If cursor is at beginning of first line, nothing to do
  if (E.cx == 0 & E.cy == 0) return;

  // Find the row where the cursor is
  Erow *row = &E.row[E.cy];
  // If there is a character to the left of the cursor, delete character and move cursor
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  // If at first character in file, try to delete implicit '\n' character
  } else {
    // Update x position of cursor to end of row above
    E.cx = E.row[E.cy - 1].size;
    // Add contents of current row to row above
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    // Delete current row
    editorDelRow(E.cy);
    // Update y position of cursor
    E.cy--;
  }
}

/*** file i/o ***/
// Convert rows in editor to a single string
char *editorRowsToString(int *buflen) {
  // Determine how long the string should be
  int totallen = 0;
  int i;
  for (i = 0; i < E.numrows; i++) {
    // Add length of each row (+1 for newline character that must follow each row)
    totallen += E.row[i].size + 1;
  }
  *buflen = totallen;

  // Allocate space for the string
  char *buf = malloc(totallen);
  // Start at beginning of buffer and copy over each row
  char *p = buf;
  for (i = 0; i < E.numrows; i++) {
    memcpy(p, E.row[i].chars, E.row[i].size);
    p += E.row[i].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  // Stores copy of filename
  free(E.filename);
  E.filename = strdup(filename);

  // Open and read a given file
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    die("fopen");
  }
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  // Keep reading the file until you reach the end of the file
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    /* Decrease the length of linelen while it is greater than zero and the
       last character in the line is a newline character or a return character.
    */
    while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen - 1] == '\r')) {
      linelen--;
    }
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

/*
Save contents of editor to file
*/
void editorSave() {
  // Prompt for filename
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)");
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
  };

  int len;
  char *buf = editorRowsToString(&len);

  /* Open file so it is available to read and write. If there is not an existing
     file one will be created. 0644 is the estandard permission code for text files.
     This code allows owner to read and write and others to only read the file.
  */
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  // If successfully opened file, then write to file
  if (fd != -1) {
    // Prevents owner from overwriting other data.
    // ftruncate sets the file's size to specified length
    if (ftruncate(fd, len) != -1) {
      // Write to file, check that the correct number of bytes was written
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  // Show error message
  free(buf);
  editorSetStatusMessage("Failed to save. I/O ERROR: %s", strerror(errno));
}

/*** input ***/
char *editorPrompt(char *prompt) {
  // Dynamically allocate buffer for user input
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  // Initialize as empty string
  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    // Show prompt
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    // Keep reading characters
    int c = editorReadKey();
    // Let user press delete/backspace in input prompt
    if (c == DELETE_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    }
    // Let user hit Esc to cancel input prompt
    if (c == '\x1b') {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    }
    // If user hits enter, clear prompt and stop reading
    if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        return buf;
      }
      // Otherwise, if user didn't use a CTRL command and ensure character is a character
    } else if (!iscntrl(c) && c < 128) {
      // Double buffersize if buffer has reached maximum capacity
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      // Add to buffer
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
  }
}
void editorMoveCursor(int key) {
  // Gets the current row that the cursor is on, or sets the current row to null
  Erow* row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    // Determine which was to more cursor depending on which key you press.
    case ARROW_LEFT:
      // Check if you are at the left edge of window
      if (row && E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      // Check if you are at the top of the window
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      // Check if you are at the bottom of the window
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : & E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}
/*
Waits for one key press and handles it.
*/
void editorProcessKeypress() {
  static int quit_times = NUCLEUS_QUIT_TIMES;
  int c = editorReadKey();
  switch (c) {
    case '\r':
      editorInsertNewLine();
      break;

    // Check for confirmation by asking user to press CTRL+Q 3 times
    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING: File has unsaved changes. Press CTRL-Q %d more times to quit.", quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      if (E.cy < E.numrows) {
        E.cx = E.row[E.cy].size;
      }
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DELETE_KEY:
      // Move cursor
      if (c == DELETE_KEY) editorMoveCursor(ARROW_RIGHT);
      // Delete character
      editorDelChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenRows - 1;
          if (E.cy > E.numrows) {
            E.cy = E.numrows;
          }
        }
        int times = E.screenRows;
        while (times--){
          /* If you press the page up key it will continue to move up until the
             cursor hits the top. If the page down arrow was pressed it will
             continue to move down until the sursor hits the bottom. */
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
      }
      break;
    // Moves cursor around depending on which arrow key you press
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }

  quit_times = NUCLEUS_QUIT_TIMES;
}

/*** output ***/
void editorScroll() {
  // Determine the x position of the cursor, based off of the render position
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  // If the cursor is above visible window, then scroll up to where cursor is
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  // If cursor is past bottom of visible window, then scroll down
  if (E.cy >= E.rowoff + E.screenRows) {
    E.rowoff = E.cy - E.screenRows + 1;
  }

  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screenCols) {
    E.coloff = E.rx - E.screenCols + 1;
  }
}

void editorDrawRows(Abuf *ab) {
  for (int y = 0; y < E.screenRows; y++) {
    // Get the row of the file that you want to display at each y position
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows){
      // Display welcome message for users
      if (E.numrows == 0 && y == E.screenRows/3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
            "Nucleus Editor -- version %s", NUCLEUS_VERSION);
        if (welcomelen > E.screenCols) {
          welcomelen = E.screenCols;
        }
        // Center welcome message
        // Find the center of the screen
        int padding = (E.screenCols - welcomelen)/2;
        if (padding) {
          abAppend(ab, "~", 1);
          // Deleting padding
          padding--;
        }
        while (padding--) {
          abAppend(ab, " ", 1);
        }
        abAppend(ab, welcome, welcomelen);
      } else {
        // Draw a column of tildes on the lefthand side of the screen
        abAppend(ab, "~", 1);
      }
    } else {
      // Determine where to draw, accounting for column offset
      int len = E.row[filerow].rsize - E.coloff;
      // User scrolled past the end of the line
      if (len < 0) {
        len = 0;
      }
      if (len > E.screenCols) {
        len = E.screenCols;
      }
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }


    // Clears lines one at a time
    abAppend(ab, "\x1b[K", 3);

    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(Abuf *ab) {
  // Inverts colors
  // Use escape key sequence M - Select Graphic Rendition
  // Option 7 is inverting colors
  abAppend(ab, "\x1b[7m", 4);

  // Write status message, with the current filename (or [No Name] if no filename),
  // as well as current line number & whether the file has been modified
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "MODIFIED": " ");
  // Determine render length
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
    E.cy + 1, E.numrows);
  // Cut status string short if it doesn't fit inside window
  if (len > E.screenCols) {
    len = E.screenCols;
  }
  // Add status message to buffer
  abAppend(ab, status, len);

  // Draw message
  while (len < E.screenCols) {
    if (E.screenCols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }

  // Resets colors
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

/*
Draw message bar.
*/
void editorDrawMessageBar(Abuf *ab) {
  // Clear message bar
  abAppend(ab,"\x1b[K", 3);
  // Check if status message is too long
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screenCols) {
    msglen = E.screenCols;
  }
  // Add message to buffer if it is <5s old
  if (msglen && time(NULL) - E.statusmsg_time < 5) {
    abAppend(ab, E.statusmsg, msglen);
  }
}

void editorRefreshScreen() {
  editorScroll();
  // do not think "\x1b[?25 is supported in our termial, so leaving it commented out"
  Abuf ab = ABUF_INIT;
  // abAppend(&ab, "\1xb[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy-E.rowoff) +1, (E.rx - E.coloff) + 1 );
  abAppend(&ab, buf, strlen(buf));
  // abAppend(&ab, "\1xb[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*
Set status message, as a variadic function.
*/
void editorSetStatusMessage(const char *fmt, ...) {
  // Access all arguments
  va_list ap;
  va_start(ap, fmt);
  // Write format string to the status message field of the editor
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** init ***/

void initEditor() {
  // Set original placement of the cursor
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.numrows = 0;
  E.row = NULL;
  E.rowoff = 0;
  E.coloff = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.dirty = 0;

  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
    die("getWindowSize");
  }
  E.screenRows -= 2;
}

int main(int argc, char *argv[]) {
  /* Want to disable canonical mode and turn on raw mode so that we can process
  each keypress as it comes in */
  enableRawMode();
  initEditor();

  // Open file, if there is one
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  // Set initial status message
  editorSetStatusMessage("HELP: CTRL + S = SAVE | CTRL + Q = QUIT");

  // Continuously refresh the screen and process key presses
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
