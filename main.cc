#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum EditorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
};

/* TODO change this to be not global (if possible) */
struct EditorConfig {
    int c_x, c_y;
    int screen_rows, screen_cols;
    struct termios original_termios;
};

/* TODO move this to another file */
struct a_buf {
    char *raw;
    int len;

    a_buf() : raw(NULL), len(0) {}

    ~a_buf() {
        this->free();
    }

    void
    append(const char *s, int len) {
        /* TODO find which c++ converter goes here */
        char *n_raw = (char *)realloc(this->raw, this->len + len);

        if (n_raw == NULL)
            return;

        memcpy(&n_raw[this->len], s, len);
        this->raw = n_raw;
        this->len += len;
    }

    void
    free() {
        ::free(this->raw);
    }
};

struct EditorConfig E;

void die(const char *s);
void disable_raw_mode();
void editor_draw_rows(struct a_buf *ab);
void editor_move_cursor(int key);
void editor_process_keypress();
int editor_read_key();
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

void editor_draw_rows(struct a_buf *ab) {
    for (int i = 0; i < E.screen_rows; i++) {
        if (i == E.screen_rows / 3) {
            char welcome[80];
            char welcome_len = snprintf(welcome, sizeof(welcome),
                    "uiua ide -- version %s", VERSION);
            
            if (welcome_len > E.screen_cols)
                welcome_len = E.screen_cols;

            int padding = (E.screen_cols - welcome_len) / 2;
            if (padding) {
                ab->append("~", 1);
                padding--;
            }

            while (padding--)
                ab->append(" ", 1);

            ab->append(welcome, welcome_len);
        } else {
            ab->append("~", 1);
        }

        ab->append("\x1b[K", 3);
        if (i < E.screen_rows - 1) {
            ab->append("\r\n", 2);
        }
    }
}

void
editor_move_cursor(int key) {
    switch (key) {
    case ARROW_LEFT:
        if (E.c_x != 0)
            E.c_x--;
        break;
    case ARROW_RIGHT:
        if (E.c_x != E.screen_cols - 1)
            E.c_x++;
        break;
    case ARROW_UP:
        if (E.c_y != 0)
            E.c_y--;
        break;
    case ARROW_DOWN:
        if (E.c_y != E.screen_rows - 1)
            E.c_y++;
        break;
    }
}

void
editor_process_keypress() {
    int c = editor_read_key();

    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        
        exit(0);
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editor_move_cursor(c);
        break;
    }
}

int
editor_read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';

        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';

                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '3': return DEL_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                }
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

void
editor_refresh_screen() {
    struct a_buf ab;

    ab.append("\x1b[?25l", 6);
    ab.append("\x1b[H", 3);

    editor_draw_rows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.c_y + 1, E.c_x + 1);
    ab.append(buf, strlen(buf));

    ab.append("\x1b[?25h", 6);
    
    write(STDOUT_FILENO, ab.raw, ab.len);
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
    E.c_x = 0;
    E.c_y = 0;

    if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
        die("get_window_size");
}
