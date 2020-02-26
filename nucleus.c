#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>

// Structure to represent the terminal
struct termios orig_termios;

void disableRawMode() {
  // Sets the attributes of the terminal to the original terminal to restore
  // the original state.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  // Gets the attributes of the current terminal
  tcgetattr(STDIN_FILENO, &orig_termios);
  // Makes sure to disable raw mode and return terminal to original state on exit
  atexit(disableRawMode);

  // Essentially copies the attributes of the current terminal
  // (before enabling raw mode)
  struct termios raw = orig_termios;
  /* Turns off the appropriate flags by modifying the structure.*/
  /*Specifically, turns off the appropriate "local flags", or basically
  miscellaneous flags.
  Turns off the automatic responses to special characters (like CTRL+C and CTRL+Z) by turning off the ISIG
  Turns off the displaying of characters being typed on the terminal by turning off ECHO
  Fun fact!/realization! sudo also turns ECHO off.
  Turns off canonical mode by turning off ISCANON.
  Turns off automatic responses to characters like CTRL+V and CTRL+O by turning
  IEXTEN off.
  &= is a bitwise and assignment. By using the bitwise-NOT operator (~), we can
  turn these bitflags off, and then by using the bitwise-AND assignment, we can
  flip the bits corresponding to these flags and retain the other bits. */
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  /* Similar to the process of updating the local flags, turns off of the
  appropriate input flags.
  Turns off responses to CTRL+S and CTRL+Q by turnign off IXON controls the
  pausing and resuming of transmission (which is also what CTRL+S and CTRL+Q control).
  Fixes CTRL+M by preventing the terminal from translating carriage returns to new line.*/
  raw.c_iflag &= ~(IXON | ICRNL);

  // Updates the terminal characteristics
  /* Resets the pointer to the termios object (which represents
  the characteristics of the terminal) to raw, the termios object we created
  which has the updated characteristics of raw mode.
  The TCSAFLUSH parameter means that these changes will only occur after all
  the output from STDIN_FILENO has been transmitted appropriately and all input
  that has been received but not read has been discarded (hence FLUSH).*/
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  /* Want to disable canonical mode and turn on raw mode so that we can process
  each keypress as it comes in */
  enableRawMode();

  /* Read user input one byte at a time into character c.
  Reads from standard input but considers standard input like a file, so
  that we can use the read() function to determine if there are still any
  unread bytes in the file */
  char c;
  // Keeps reading while there are more bytes to read and
  // while the user hasn't pressed q
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    // If the character is a control character, then we don't want to print it
    if (iscntrl(c)) {
      printf("%d\n", c);
    // Otherwise, print the ASCII code of the character and the character itself
    } else {
      printf("%d ('%c')\n", c,c);
    }
  }
  return 0;
}

//Why is CRT+M = 10, not 13. CRT+J = 10. <--- Shreya couldn't figure this out either, so we should ask Steve!
