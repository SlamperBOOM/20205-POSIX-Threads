#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include "socket_operations.h"

int hostname_to_ip(char* hostname, char* ip) {
    struct addrinfo hints, * servinfo, * p;
    struct sockaddr_in* h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
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

ssize_t receive_response(int sock, char* buf, size_t buf_len) {
    ssize_t total_bytes_read = 0;
    while(total_bytes_read < buf_len) {
        ssize_t bytes_read = read(sock, buf + total_bytes_read, buf_len - total_bytes_read);
        if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            return -1;
        }
        total_bytes_read += bytes_read;
    }
    return total_bytes_read;
}
