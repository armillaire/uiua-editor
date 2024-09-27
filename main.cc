#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1f)

/* TODO change this to be not global (if possible) */
struct EditorConfig {
    int screen_rows, screen_cols;
    struct termios original_termios;
};

struct EditorConfig E;

void die(const char *s);
void disable_raw_mode();
void editor_draw_fringe();
void editor_process_keypress();
char editor_read_key();
void editor_refresh_screen();
void enable_raw_mode();
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
void init_editor();

int
main() {
    enable_raw_mode();
    init_editor();

    while (1) {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;

}

void
die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void
disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1)
        die("tcsetattr");
}

/* prints the fringe of ~ on the edges */
void editor_draw_fringe() {
    for (int i = 0; i < E.screen_cols; i++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void
editor_process_keypress() {
    char c = editor_read_key();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            
            exit(0);
            break;
    }
}

char
editor_read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    return c;
}

void
editor_refresh_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    editor_draw_fringe();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

void
enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1)
        die("tcgetattr");

    atexit(disable_raw_mode);

    struct termios raw = E.original_termios;
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
get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;

        if (buf[i] == 'R')
            break;

        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

int
get_window_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;

        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void
init_editor() {
    if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
        die("get_window_size");
}
