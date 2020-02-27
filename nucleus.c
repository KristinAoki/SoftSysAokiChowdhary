#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

// Structure to represent the terminal
struct termios orig_termios;

void die(const char *s) {
  // prints descriptive error message for the gloabl errno variable,
  // that is set when something fails
  perror(s);
  exit(1);
}

/*
Restores the attributes of the terminal to the original state.
*/
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tcsetattr");
  }
}

/*
Changes the attributes of the terminal to enable raw mode.
*/
void enableRawMode() {
  // Gets the attributes of the current terminal
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  // Makes sure to disable raw mode and return terminal to original state on exit
  atexit(disableRawMode);

  // Essentially copies the attributes of the current terminal
  // (before enabling raw mode)
  struct termios raw = orig_termios;

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

int main() {
  /* Want to disable canonical mode and turn on raw mode so that we can process
  each keypress as it comes in */
  enableRawMode();

  // Keeps reading while there are more bytes to read and
  // while the user hasn't pressed q
  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    // If the character is a control character, then we don't want to print it
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    // Otherwise, print the ASCII code of the character and the character itself
    } else {
      printf("%d ('%c')\r\n", c,c);
    }
    if (c == 'q') break;
  }
  return 0;
}
