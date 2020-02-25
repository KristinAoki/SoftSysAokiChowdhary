#include <unistd.h>
#include <termios.h>

void enableRawMode() {
  
  raw.c_lflag &= ~(ECHO);
}

int main() {
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    /* code */
  }
  return 0;
}
