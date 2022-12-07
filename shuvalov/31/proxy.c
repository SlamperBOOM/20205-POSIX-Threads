#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "picohttpparser-master/picohttpparser.h"
#include "socket_operations/socket_operations.h"

#define SA struct sockaddr
#define PORT 80
#define PROXY_PORT 12000

extern int errno;

void get_hostname(const char** hostname, size_t* hostname_len, struct phr_header* headers, size_t num_headers) {
    for (size_t i = 0; i < num_headers; i++) {
        if (strncmp(headers[i].name, "Host", headers[i].name_len) == 0) {
            *hostname = headers[i].value;
            *hostname_len = headers[i].value_len;
            return;
        }
    }
    *hostname = NULL;
}

int main(int argc, char* argv[]) {
    int proxy_sock_fd, conn_fd;
    struct sockaddr_in proxyaddr;

    // socket create and verification
    proxy_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_sock_fd == -1) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    } else {
        printf("Socket successfully created..\n");
    }
    bzero(&proxyaddr, sizeof(proxyaddr));

    // assign IP, PORT
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    proxyaddr.sin_port = htons(PROXY_PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(proxy_sock_fd, (SA*) &proxyaddr, sizeof(proxyaddr))) != 0) {
        printf("socket bind failed...\n");
        close(proxy_sock_fd);
        return EXIT_FAILURE;
    } else {
        printf("Socket successfully bound..\n");
    }
    // Now server is ready to listen and verification
    if ((listen(proxy_sock_fd, 5)) != 0) {
        perror("listen failed");
        close(proxy_sock_fd);
        return EXIT_FAILURE;
    } else {
        printf("Server listening..\n");
    }
    // Accept the data packet from client and verification
    conn_fd = accept(proxy_sock_fd, NULL, NULL);
    if (conn_fd < 0) {
        perror("server accept failed");
        close(proxy_sock_fd);
        return EXIT_FAILURE;
    } else {
        printf("server accept the client...\n");
    }
    // Function for chatting between client and server

    // After chatting close the socket


    char buf[4096], * method, * path;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t buf_len = 0, prev_buf_len = 0, method_len, path_len, num_headers;
    ssize_t rret;
    int j = 0;
    while (1) {
        fprintf(stderr, "Iteration %d\n", j++);
        /* read the request */
        while ((rret = read(conn_fd, buf + buf_len, sizeof(buf) - buf_len)) == -1 && errno == EINTR);
        if (rret <= 0) {
            close(conn_fd);
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
            break; /* successfully parsed the request */
        } else if (pret == -1) {
            close(conn_fd);
            fprintf(stderr, "phr_parse_request error\n");
            return EXIT_FAILURE;
        }
        /* request is incomplete, continue the loop */
        assert(pret == -2);
        if (buf_len == sizeof(buf)) {
            close(conn_fd);
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
    get_hostname((const char**) &hostname, &hostname_len, headers, num_headers);
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
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    } else {
        printf("connected to the server..\n");
    }
    send_request(sockfd, buf, buf_len);
    size_t receive_buf_len = 1024;
    char* receive_buf = (char*) malloc(sizeof(char) * receive_buf_len);
    ssize_t bytes_read = receive_response(sockfd, receive_buf, receive_buf_len);
    printf("Bytes read: %zd\nResponse:\n%.*s\n\n", bytes_read, (int) bytes_read, receive_buf);
    send_request(conn_fd, receive_buf, bytes_read);
    close(sockfd);
    close(conn_fd);
    close(proxy_sock_fd);
    free(receive_buf);
    return EXIT_SUCCESS;
}
