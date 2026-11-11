#include "console.h"
#include "memory.h"
#include "process.h"
#include "timer.h"

#define SHELL_LINE_MAX 128
#define SHELL_BACKSPACE 0x7FU

void kernel_halt(int status);
void kernel_break(void);
void initrd_list(void);
int initrd_cat(const char *name);
int run_named(const char *name);

static int string_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) return 0;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static int starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text != *prefix) return 0;
        ++text;
        ++prefix;
    }
    return 1;
}

static void read_line(char *buffer, int capacity) {
    int length = 0;
    for (;;) {
        int character = console_getc();
        if (character < 0) {
            continue;
        }
        if (character == '\n') {
            console_putc('\n');
            buffer[length] = '\0';
            return;
        }
        if (character == SHELL_BACKSPACE || character == '\b') {
            if (length > 0) {
                --length;
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }
        if (character >= 32 && character <= 126 && length + 1 < capacity) {
            buffer[length] = (char)character;
            ++length;
            console_putc((char)character);
        }
    }
}

static void run_command(const char *line) {
    if (line[0] == '\0') {
        return;
    }
    if (string_equal(line, "help")) {
        console_write("help echo clear mem ps ls cat run exit\n");
    } else if (starts_with(line, "echo")) {
        const char *argument = line + (sizeof("echo") - 1);
        while (*argument == ' ') ++argument;
        console_write(argument);
        console_putc('\n');
    } else if (string_equal(line, "clear")) {
        console_clear();
    } else if (string_equal(line, "mem")) {
        console_write("ram=128MiB pages=32768 used=");
        console_write_uint(memory_used_pages());
        console_write(" heap=");
        console_write_uint(heap_used());
        console_write(" ticks=");
        console_write_uint(timer_ticks());
        console_putc('\n');
    } else if (string_equal(line, "ps")) {
        process_list();
    } else if (string_equal(line, "ls")) {
        initrd_list();
    } else if (starts_with(line, "cat ")) {
        const char *name = line + (sizeof("cat") - 1);
        if (initrd_cat(name) != 0) {
            console_write("cat: file not found\n");
        }
    } else if (starts_with(line, "run")) {
        const char *name = line + (sizeof("run") - 1);
        while (*name == ' ') ++name;
        if (*name == '\0') name = "init";
        int status = run_named(name);
        if (status < 0) {
            console_write("run: program not found\n");
        } else {
            console_write("exit status ");
            console_write_uint((unsigned int)status);
            console_putc('\n');
        }
    } else if (string_equal(line, "crash")) {
        kernel_break();
    } else if (string_equal(line, "exit")) {
        kernel_halt(0);
    } else {
        console_write("unknown command: ");
        console_write(line);
        console_putc('\n');
    }
}

typedef char shell_line_t[128]; /* SHELL_LINE_MAX */

void shell_run(void) {
    shell_line_t line;
    for (;;) {
        console_write("> ");
        read_line(line, (int)sizeof(line));
        run_command(line);
    }
}
