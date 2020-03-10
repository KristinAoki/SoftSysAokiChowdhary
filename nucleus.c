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

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}
#define NUCLEUS_VERSION "0.0.1"

// enum to define constants for the arrow keys, etc
enum editorKey {
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
  char *chars;
} Erow;

// Structure to represent the editor state
/* struct fields:
- struct termios orig_termios - termios object that represents the terminal
- int screenRows - the number of rows on the screen
- int screenCols - the number of columns on the screen
- int cx, cy - the x & y coordinates of the cursor
- int numRows - the number of rows to text
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
  int numrows;
  Erow *row;
  int rowoff;
  int coloff;
} Editor;

Editor E;

typedef struct abuf {
  char *b;
  int len;
} Abuf;

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
  E.numrows++;
}

/*** file i/o ***/
void editorOpen(char *filename) {
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
    // Decrease the length of linelen while it is greater than zero and the
    // last character in the line is a newline character or a return character.
    while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen - 1] == '\r')) {
      linelen--;
    }
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
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

  // TODO: Refactor to a function? getCurrentRow?
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
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      E.cx = E.screenCols-1;
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
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
  }
}

/*** output ***/
void editorScroll() {
  // If the cursor is above visible window, then scroll up to where cursor is
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  // If cursor is past bottom of visible window, then scroll down
  if (E.cy >= E.rowoff + E.screenRows) {
    E.rowoff = E.cy - E.screenRows + 1;
  }

  if (E.cx < E.coloff) {
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screenCols) {
    E.coloff = E.cx - E.screenCols + 1;
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
      int len = E.row[filerow].size - E.coloff;
      // User scrolled past the end of the line
      if (len < 0) {
        len = 0;
      }
      if (len > E.screenCols) {
        len = E.screenCols;
      }
      abAppend(ab, &E.row[filerow].chars[E.coloff], len);
    }


    // Clears lines one at a time
    abAppend(ab, "\x1b[K", 3);

    if (y < E.screenRows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  editorScroll();
  // do not think "\x1b[?25 is supported in our termial, so leaving it commented out"
  Abuf ab = ABUF_INIT;
  // abAppend(&ab, "\1xb[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy-E.rowoff) +1, (E.cx - E.coloff) + 1 );
  abAppend(&ab, buf, strlen(buf));
  // abAppend(&ab, "\1xb[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init ***/

void initEditor() {
  // Set original placement of the cursor
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  E.row = NULL;
  E.rowoff = 0;
  E.coloff = 0;

  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
    die("getWindowSize");
  }
}

int main(int argc, char *argv[]) {
  /* Want to disable canonical mode and turn on raw mode so that we can process
  each keypress as it comes in */
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  // Keeps reading while there are more bytes to read and
  // while the user hasn't pressed q
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
