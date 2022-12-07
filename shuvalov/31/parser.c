#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include "picohttpparser-master/picohttpparser.h"

#define SA struct sockaddr
#define PORT 80

extern int errno;

void get_host(const char** hostname, size_t* hostname_len, struct phr_header* headers, size_t num_headers) {
    for (size_t i = 0; i < num_headers; i++) {
        if (strncmp(headers[i].name, "Host", headers[i].name_len) == 0) {
            *hostname = headers[i].value;
            *hostname_len = headers[i].value_len;
            return;
        }
    }
    *hostname = NULL;
}

int hostname_to_ip(char* hostname, char* ip) {
    struct addrinfo hints, * servinfo, * p;
    struct sockaddr_in* h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, "http", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in*) p->ai_addr;
        strcpy(ip, inet_ntoa(h->sin_addr));
    }

    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}

void send_request(int sock, const char* buf, size_t buf_len) {
    size_t bytes_written = 0;
    while (bytes_written < buf_len) {
        bytes_written = write(sock, buf + bytes_written, buf_len - bytes_written);
    }
}

void receive_response(int sock, char* buf, size_t buf_len) {
    size_t bytes_read = 0;
    while(bytes_read < buf_len) {
        bytes_read = read(sock, buf + bytes_read, buf_len - bytes_read);
    }

    printf("Response: %.*s", (int) bytes_read, buf);
}

int main(int argc, char* argv[]) {
    char buf[4096], * method, * path;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t buf_len = 0, prev_buf_len = 0, method_len, path_len, num_headers;
    ssize_t rret;
    int sock;
    sock = open("test.txt", O_RDONLY);
    int j = 0;
    while (1) {
        fprintf(stderr, "Iteration %d\n", j++);
        /* read the request */
        while ((rret = read(sock, buf + buf_len, sizeof(buf) - buf_len)) == -1 && errno == EINTR);
        if (rret <= 0) {
            close(sock);
            perror("read");
            return EXIT_FAILURE;
        }
        prev_buf_len = buf_len;
        buf_len += rret;
        /* parse the request */
        num_headers = sizeof(headers) / sizeof(headers[0]);
        pret = phr_parse_request(buf, buf_len, (const char**) &method,
                                 &method_len, (const char**) &path, &path_len,
                                 &minor_version, headers, &num_headers, prev_buf_len);
        fprintf(stderr, "pret = %d\n", pret);

        if (pret > 0) {
            fprintf(stderr, "successfully parsed\n");
            close(sock);
            break; /* successfully parsed the request */
        } else if (pret == -1) {
            close(sock);
            fprintf(stderr, "phr_parse_request error\n");
            return EXIT_FAILURE;
        }
        /* request is incomplete, continue the loop */
        assert(pret == -2);
        if (buf_len == sizeof(buf)) {
            close(sock);
            fprintf(stderr, "request is too long\n");
            return EXIT_FAILURE;
        }
    }

    printf("\n\n");

    printf("request is %d bytes long\n", pret);
    printf("method is %.*s\n", (int) method_len, method);
    printf("path is %.*s\n", (int) path_len, path);
    printf("HTTP version is 1.%d\n", minor_version);
    printf("headers:\n");
    for (int i = 0; i != num_headers; ++i) {
        printf("%.*s: %.*s\n", (int) headers[i].name_len, headers[i].name,
               (int) headers[i].value_len, headers[i].value);
    }

    char* hostname;
    size_t hostname_len;
    get_host((const char**) &hostname, &hostname_len, headers, num_headers);
    hostname[hostname_len] = '\0';
    printf("\nHostname: %s\n", hostname);

    char ip[100];
    hostname_to_ip(hostname, ip);
    printf("IP: %s\n", ip);

    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    } else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*) &servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    } else {
        printf("connected to the server..\n");
    }
    send_request(sock, buf, buf_len);
    receive_response(sock, buf, buf_len);
    close(sockfd);
    exit(EXIT_SUCCESS);
}