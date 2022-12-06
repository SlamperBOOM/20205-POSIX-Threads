#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>

#define POLL_TIMEOUT (1000)

int running = 1;

int ParseInt(char* str) {
    char * endpoint;
    int res = (int)strtol(str, &endpoint, 10);

    if (*endpoint != '\0') {
        fprintf(stderr, "Unable to parse: '%s' as int\n", str);
        exit(1);
    }
    return res;
}

void SigHandler(int id) {
    if (id == SIGINT) {
        running = 0;
    }
}

int WriteToFd(int fd, char* content, int content_size) {
    int write_size = 0;
    while (write_size < content_size) {
        int write_count = (int)write(fd, content + write_size, content_size - write_size);
        if (write_count == -1) {
            return 1;
        }
        write_size += write_count;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if(argc != 3) {
        fprintf(stderr,"Specify exact 2 args: ip, port");
        return 1;
    }
    int port = ParseInt(argv[2]);


    struct sigaction sa;
    sa.sa_handler = SigHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Error while create socket \n");
        return 1;
    }


    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    int status = inet_pton(AF_INET, argv[1], &serverAddress.sin_addr);
    if (status <= 0) {
        fprintf(stderr,"Error while parse address=%s\n", argv[1]);
        return 1;
    }

    status = connect(socket_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (status < 0){
        fprintf(stderr, "Error while connect failed\n");
        return 1;
    }

    struct pollfd poll_fd[2];
    poll_fd[0].fd = socket_fd;
    poll_fd[0].events = POLLIN | POLLHUP;

    poll_fd[1].fd = STDIN_FILENO;
    poll_fd[1].events = POLLIN;

    char line_input[BUFSIZ];
    char socket_input[BUFSIZ];
    while (running) {

        poll_fd[0].revents = 0;
        poll_fd[1].revents = 0;

        int poll_res = poll(poll_fd, 2, POLL_TIMEOUT);

        if (poll_res == 0) {
            continue;
        }

        if (poll_res == -1 && errno != EINTR) {
            perror("Error while poll");
            close(socket_fd);
            return 1;
        }

        if (poll_fd[0].revents && POLLIN) {
            int read_count = (int) read(socket_fd, socket_input, BUFSIZ);
            WriteToFd(STDOUT_FILENO, socket_input, read_count);
        }

        if (poll_fd[1].revents && POLLIN) {
            int read_count = (int) read(STDIN_FILENO, line_input, BUFSIZ);
            if (line_input[read_count - 1] == '\n') {
                read_count--;
            }
            WriteToFd(socket_fd, line_input, read_count);
        }
    }

    char end_symbol = '\0';
    int err = WriteToFd(socket_fd, &end_symbol, 1);
    if (err != 0) {
        fprintf(stderr, "Error while write to socket\n");
    }
    printf("Exit client\n");
    close(socket_fd);

    return 0;
}