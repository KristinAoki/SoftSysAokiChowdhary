/*** includes ***/
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

/*** data ***/
// Structure to represent the editor state
typedef struct editorConfig {
  // Structure to represent the terminal
  struct termios orig_termios;
  int screenRows;
  int screenCols;
} editor;

editor E;

struct abuf {
  char *b;
  int len;
};

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
char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
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

/*** input ***/
/*
Waits for one key press and handles it.
*/
void editorProcessKeypress() {
  char c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

/*** output ***/
void editorDrawRows(struct abuf *ab) {
  // Draw a column of tildes on the lefthand side of the screen
  int y;
  for (y = 0; y < E.screenRows; y++) {
    abAppend(ab, "~", 1);

    if (y < E.screenRows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[2J", 4);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  abAppend(&ab, "\x1b[H", 3);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init ***/

void initEditor() {
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}

int main() {
  /* Want to disable canonical mode and turn on raw mode so that we can process
  each keypress as it comes in */
  enableRawMode();
  initEditor();

  // Keeps reading while there are more bytes to read and
  // while the user hasn't pressed q
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
