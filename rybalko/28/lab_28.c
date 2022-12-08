#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>

#define SELECT_SEC 10
#define SELECT_USEC 0
#define LINES_PER_SCREEN 26
#define BUF_SIZE 1048576
#define HTTP_PORT 80
#define BODY_DELIM "\r\n\r\n"
#define HTTP_DELIM "://"

struct termios saved_attr;
int tty_fd = STDIN_FILENO;
int socket_fd;
int largest_fd;

void RestoreTerminal() {
    write(STDOUT_FILENO, "\033[1;31mClose connection!\033[0m\n", 30);
    if (tcsetattr(tty_fd, TCSANOW, &saved_attr) == -1) {
        write(STDOUT_FILENO, "Error in tcsetattr(3)!\n", 23);
        exit(EXIT_FAILURE);
    }
}

void SigintHandle() {
    close(socket_fd);
    RestoreTerminal();
    exit(EXIT_SUCCESS);
}

void PrintServiceMessage(char* str) {
    printf("\033[0;32m"); //green
    printf("%s\n", str);
    printf("\033[0m"); //restore
}

void ParseFullURL(char* host, char* url, int* port, char* full_url) {
    char* url_without_http = strstr(full_url, HTTP_DELIM);
    if (url_without_http == NULL) {
        url_without_http = full_url;
    } else {
        url_without_http += strlen(HTTP_DELIM);
    }
    char* url_tmp = strpbrk(url_without_http, ":/");
    if (url_tmp == NULL) {
        strcpy(host, url_without_http);
        strcpy(url, "/");
    } else {
        if (url_tmp[0] == '/') {
            strcpy(url, url_tmp);
        } else {
            *port = atoi(url_tmp + 1);
            strcpy(url, "/");
        }
        int host_end_id = url_tmp - url_without_http;
        url_without_http[host_end_id] = '\0';
        strcpy(host, url_without_http);
    }
}

void SetFDs(fd_set* fds) {
    FD_ZERO(fds);
    FD_SET(socket_fd, fds);
    FD_SET(tty_fd, fds);
}

int PrintHTTPContents(char* full_url) {
    char host[strlen(full_url)];
    char url[strlen(full_url)];
    int port = HTTP_PORT;
    ParseFullURL(host, url, &port, full_url);
    printf("\033[4;34mhost:\033[0m %s\n\033[4;34murl:\033[0m %s\n\033[4;34mport:\033[0m %d\n", host, url, port);

    char *request_fmt = "GET %s HTTP/1.0\r\n\r\n";
    int request_len = strlen(url) + strlen(request_fmt);
    char request[request_len], response[BUF_SIZE];
    snprintf(request, request_len, request_fmt, url);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Error in socket(3SOCKET)!\n");
        return -2;
    }

    printf("\033[;31mGetting host...\033[0m\n");
    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        perror("Error in gethostbyname(3NSL)!\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server -> h_addr, server -> h_length);

    printf("\033[;33mConnecting...\033[0m\n");
    if (connect(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Error in connect(3SOCKET)!\n");
        return -1;
    }

    printf("\033[;32mSending request...\033[0m\n");
    int writed_bytes = write(socket_fd, request, strlen(request));
    if (writed_bytes == -1) {
        perror("Error in write(2)!\n");
        return -1;
    }

    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    int delim_size = ws.ws_col;
    char delim[delim_size + 1];
    memset(delim, '=', delim_size);
    delim[delim_size] = '\0';
    printf("\033[;35m%s\n\n\033[0m", delim);

    largest_fd = socket_fd > tty_fd ? socket_fd : tty_fd;
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    struct timeval tv;
    tv.tv_sec = SELECT_SEC;
    tv.tv_usec = SELECT_USEC;
    int lines_written = 0;
    ssize_t total_read = 0;
    ssize_t readed_bytes = 0;
    ssize_t total_written = 0;
    ssize_t written_bytes = 0;
    bool is_head_skipped = false;

    do {
        fd_set fds;
        SetFDs(&fds);
        int select_res = select(largest_fd + 1, &fds, NULL, NULL, &tv);

        if (select_res == 0) {
            continue;
        }

        if (select_res == -1) {
            perror("Error in select(2)!\n");
            return -1;
        }

        if (FD_ISSET(tty_fd, &fds) != 0) {
            char c;
            read(tty_fd, &c, 1);
            if (c == ' ') {
                lines_written = 0;
                printf("\33[A\r                          \r");
            }
        }

        if (FD_ISSET(socket_fd, &fds) != 0) {
            readed_bytes = read(socket_fd, response + total_read - total_written, BUF_SIZE - total_read + total_written);
            if (readed_bytes == -1) {
                perror("Error in read(2) while reading socket!\n");
                return -1;
            }
            total_read += readed_bytes;
        }

        if (lines_written <= LINES_PER_SCREEN) {
            if (!is_head_skipped) {
                char* delimPos = strstr(response, BODY_DELIM);
                if (delimPos != NULL) {
                    total_written = delimPos - response + strlen(BODY_DELIM);
                    is_head_skipped = true;
                }
            }
            if (is_head_skipped) {
                for (written_bytes = 0; written_bytes < total_read - total_written; written_bytes++) {
                    if (response[written_bytes] == '\n' || response[written_bytes] == '\r') {
                        lines_written++;
                        putchar('\n');
                    } else {
                        putchar(response[written_bytes]);
                    }
                    if (lines_written > LINES_PER_SCREEN) {
                        PrintServiceMessage("Press space to scroll down");
                        written_bytes++;
                        break;
                    }
                }
                total_written += written_bytes;
                memmove(response, response + written_bytes, total_read - total_written);
            }
        }
    }
    while(total_read != total_written);
    putchar('\n');
    close(socket_fd);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Not enough arguments!\nUsage: %s <url or ip:port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (!isatty(tty_fd)) {
        perror("Not a valid terminal\n");
        exit(EXIT_FAILURE);
    }

    struct termios attr;
    if (tcgetattr(tty_fd, &saved_attr) == -1) {
        perror("Error in tcgetattr(3)!\n");
        exit(EXIT_FAILURE);
    }

    attr = saved_attr;
    attr.c_lflag &= ~ECHO;
    attr.c_lflag &= ~ICANON;
    attr.c_cc[VMIN] = 1;

    if (tcsetattr(tty_fd, TCSAFLUSH, &attr) == -1) {
        perror("Error in tcsetattr(3)!\n");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = SigintHandle;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int res = PrintHTTPContents(argv[1]);
    if (res < 0) {
         printf("Error in PrintHTTPContents()!\n");
         if (res == -1) {
             close(socket_fd);
         }
    }
    RestoreTerminal();
    exit(EXIT_SUCCESS);
}
