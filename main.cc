#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_termios; /* this prolly shouldn't be global */

void
die(const char *s) {
    perror(s);
    exit(1);
}

void
disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == -1)
        die("tcsetattr");
}

void
enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
        die("tcgetattr");

    atexit(disable_raw_mode);

    struct termios raw = original_termios;
    raw.c_iflag &= ~(  BRKINT /* disabling break conditions becoming SIGINT */
                     | ICRNL  /* disabling CR -> NL */
                     | INPCK  /* disabling parity checking */
                     | ISTRIP /* disabling smthn prolly already disabled */
                     | IXON   /* disabling control flow signals */);

    raw.c_oflag &= ~(  OPOST  /* disabling output processing */);

    raw.c_cflag &= (CS8);     /* sets character size to 8 bits */

    raw.c_lflag &= ~(  ECHO   /* disabling echo */
                     | ICANON /* disabling canonical mode */
                     | IEXTEN /* disabling literal character sending */
                     | ISIG   /* disabling SIGINT */);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int
main() {
    enable_raw_mode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");

        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q')
            break;
    }

    return 0;
}
