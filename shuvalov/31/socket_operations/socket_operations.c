#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include "socket_operations.h"

int hostname_to_ip(char* hostname, char* ip) {
    struct addrinfo hints, * servinfo;
    struct sockaddr_in* h;
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(hostname, "http", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in*) p->ai_addr;
        strcpy(ip, inet_ntoa(h->sin_addr));
    }
    freeaddrinfo(servinfo);
    return 0;
}

ssize_t write_all(int sock, const char* buf, size_t buf_len) {
    ssize_t total_bytes_written = 0;
    while (total_bytes_written < buf_len) {
        ssize_t bytes_written = write(sock, buf + total_bytes_written, buf_len - total_bytes_written);
        if (bytes_written == 0) {
            break;
        } else if (bytes_written == -1) {
            return -1;
        }
        total_bytes_written += bytes_written;
    }
    return total_bytes_written;
}

ssize_t read_all(int sock, char* buf, size_t buf_len) {
    ssize_t total_bytes_read = 0;
    while(total_bytes_read < buf_len) {
        ssize_t bytes_read = read(sock, buf + total_bytes_read, buf_len - total_bytes_read);
        if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            return -1;
        }
        total_bytes_read += bytes_read;
        printf("Total bytes read: %zd\n", total_bytes_read);
    }
    return total_bytes_read;
}
