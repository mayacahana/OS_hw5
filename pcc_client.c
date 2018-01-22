#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>

bool isValidIpAddress(char *ipAddress);
int read_all(int fd, void* buffer, size_t bytes_to_read);
int write_all(int fd, void* buffer, size_t bytes_to_write);

#define PCC_INDEX (index)(index-32)
#define BACKLOG 10
#define NUM_PRINTABLE 95
#define RANDOM_GENERATOR "/dev/urandom"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CHUNK_SIZE 1024

// credit to https://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ip-address-in-c
bool isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

int read_all(int fd, void* buffer, size_t bytes_to_read){ 
    int total_read = 0;
    while (total_read < bytes_to_read) {
        ssize_t bytes_read = read(fd ,buffer + total_read, bytes_to_read - total_read);
        if (bytes_read < 0){
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        if (bytes_read == 0) {
            return total_read;
        }
        total_read += bytes_read;
    }
    return total_read;
}

int write_all(int fd, void* buffer, size_t bytes_to_write) {
    int total_written = 0;
    while (total_written < bytes_to_write) {
        ssize_t bytes_write = write(fd, buffer+total_written, bytes_to_write - total_written);
        if (bytes_write < 0) {
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        if (bytes_write == 0) {
            return total_written;
        }
        total_written += bytes_write;
    }
    return total_written;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("\n Error: not enough args");
        exit(-1);
    }
    // create a TCP connection 
    char* hostname = argv[1];
    unsigned short port = (unsigned short) atoi(argv[2]);
    unsigned int length = atoi(argv[3]);
    int sockfd = -1;
    //char* byte_generator = RANDON_GENERATOR;
    struct sockaddr_in server_addr;

	
    // covert internet dot address to network address
    if (isValidIpAddress(hostname) == true) {
        memset(&server_addr, '0', sizeof(server_addr));
	    server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port); // for endiannes
        char hostname_cpy[32];
        strcpy(hostname_cpy, hostname);
        server_addr.sin_addr.s_addr = inet_addr(hostname_cpy);
        // create new listen socket
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        // connect
        if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
    } else {
        struct addrinfo dns, *info_ptr;
        memset(&dns, 0, sizeof(dns));
        dns.ai_family = AF_UNSPEC;
        dns.ai_protocol = 0;
        dns.ai_flags = 0;
        int tmp = -1;
        if ((tmp = getaddrinfo(argv[1], argv[2], &dns, &info_ptr)) == 1) {
            printf("Error: %s \n", strerror(errno));
            exit(-1); 
        }
        // create new listen socket
        if ((sockfd = socket(info_ptr->ai_family, info_ptr->ai_socktype, info_ptr->ai_protocol)) < 0){
        printf("Error: %s \n", strerror(errno));
            exit(-1);
        }
        // connect
        if (connect(sockfd, (struct sockaddr *)(info_ptr->ai_addr), info_ptr->ai_addrlen) < 0) {
        printf("Error: %s \n", strerror(errno));
            exit(-1);
        }

    }
    // read from file
    int fd = open(RANDOM_GENERATOR, O_RDONLY, 0777);
    if (fd < 0) {
        printf("Error: %s \n", strerror(errno));
        exit(-1);
    }
    // write length of msg
    if (write_all(sockfd, &length, sizeof(unsigned int)) < sizeof(unsigned int)) {
        printf("Error: failed to write to sockfd\n");
        exit(-1);
    }
    //
    int bytes_to_read = MIN(CHUNK_SIZE, length);
    int cnt = 0, num_read = -1;
    char* buffer[CHUNK_SIZE];

    while (cnt < length) {
        if ((num_read = read_all(fd, buffer, bytes_to_read)) < 0) {
            printf("Error: failed to read from random file %s \n", strerror(errno));
            exit(-1);
        }
        cnt+=num_read;
        if (write_all(sockfd, buffer, num_read) < num_read) {
            printf("Error: fail to send random to server %s \n", strerror(errno));
            exit(-1);
        }
        bytes_to_read = MIN(CHUNK_SIZE, length - cnt);
        memset(buffer, 0, CHUNK_SIZE);
    }


    // get printable chars
    // read from server
    unsigned int cnt_printable = 0;
    //unsigned incoming_message = sizeof(unsigned int);
    if ((num_read = read_all(sockfd, &cnt_printable, sizeof(unsigned int))) < sizeof(unsigned int)) {
        printf("Error: fail to send random to server %s \n", strerror(errno));
        exit(-1);
    }
    printf("# of printable characters: %u\n", cnt_printable);
    // close socket
    if (close(fd) < 0){
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    if (close(sockfd) < 0){
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    return 0;

}