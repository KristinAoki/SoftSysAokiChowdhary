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
// Stores a line of text as a pointer
typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} Erow;

// Structure to represent the editor state
/* struct fields:
- struct termios orig_termios - termios object that represents the terminal
- int screenRows - the number of rows on the screen
- int screenCols - the number of columns on the screen
- int cx, cy - the x & y coordinates of the cursor
- int numrows - the number of rows to text
- Erow *erow - an array of rows
- int rowoff - the offset variable, which keeps track of the row
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

typedef struct abuf {
  char *b;
  int len;
} Abuf;

/*** prototypes ***/

// Able to call function before it is defined.
void editorSetStatusMessage(const char *fmt, ...);

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
      rx += (NUCLEUS_TAB_STOP -1) - (rx % NUCLEUS_TAB_STOP);
    }
    rx++;
  }
  rx;
}

void editorUpdateRow(Erow *row) {
  int tabs = 0;
  int i;

  for (i = 0; i < row->size; i++) {
    if (row->chars[i] == '\t') tabs++;
  }

  free(row->render);
  row->render = malloc(row->size + tabs*(NUCLEUS_TAB_STOP -1) + 1);

  int idx = 0;
  // Deterince the size of the row
  for (i = 0; i < row->size; i++) {
    if (row->chars[i] == '\t') {
      row->render[idx++] = ' ';
      while (idx % NUCLEUS_TAB_STOP != 0 ) {
        row->render[idx++] = ' ';
      }
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
void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(Erow)*(E.numrows + 1));
  int idx = E.numrows;
  E.row[idx].size = len;
  E.row[idx].chars = malloc(len+1);
  memcpy(E.row[idx].chars, s, len);
  E.row[idx].chars[len] = '\0';

  E.row[idx].rsize = 0;
  E.row[idx].render = NULL;
  editorUpdateRow(&E.row[idx]);

  E.numrows++;
  E.dirty++;
}

void editorRowInsertChar(Erow *row, int idx, int c) {
  if (idx < 0 || idx > row->size){
    idx = row->size;
  }
  // Add 2 to allow for an additional character at the end and the null terminator
  row->chars = realloc(row->chars, row->size + 2);

  // Copy a block of memory from a location to another
  memmove(&row->chars[idx + 1], &row->chars[idx], row->size - idx + 1);
  row->size++;
  row->chars[idx] = c;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operatios ***/

void editorInsertChar(int c) {
  // Add a row to botto of file if you are on the last row
  if (E.cy == E.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

/*** file i/o ***/
char *editorRowsToString(int *buflen){
  int totallen = 0;
  int i;
  for (i = 0; i < E.numrows; i++) {
    totallen += E.row[i].size + 1;
  }
  *buflen = totallen;

  char *buf = malloc(totallen);
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
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) return;

  int len;
  char *buf = editorRowsToString(&len);

  /* Open file so it is available to read and write. If there is not an existing
     file one will be created. 0644 is the estandard permission code for text files.
     This code allows owner to read and write and others only read the file.
  */
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    // Prevents owner from overwriting other data.
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len)) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }

  free(buf);
  editorSetStatusMessage("Failed to save. I/O ERROR: %s", strerror(errno));
}

/*** input ***/
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
  int c = editorReadKey();
  switch (c) {
    case '\r':
      break;

    case CTRL_KEY('q'):
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
}

/*** output ***/
void editorScroll() {
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
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines",
    E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "MODIFIED": " ");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
    E.cy + 1, E.numrows);
  if (len > E.screenCols) {
    len = E.screenCols;
  }
  abAppend(ab, status, len);
  while (len < E.screenCols) {
    if (E.screenCols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++
    }
  }
  // Resets colors
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(Abuf *ab) {
  abAppend(ab,"\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screenCols) {
    msglen = E.screenCols;
  }
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

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
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
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: CTRL + S = SAVE | CTRL + Q = QUIT");
  // Keeps reading while there are more bytes to read and
  // while the user hasn't pressed q
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
