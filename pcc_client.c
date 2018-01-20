#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>

#define PCC_INDEX (index)(index-32)
#define BACKLOG 10
#define NUM_PRINTABLE 95
#define RANDON_GENERATOR "/dev/urandom"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CHUNK_SIZE 1024
// credit to http://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/
int hostname_to_ip(char* hostname, char* ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
         
    if ((he = gethostbyname(hostname)) == NULL) {
        // get the host info
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    for(i = 0; addr_list[i] != NULL; i++) {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }
    return 1;
}

int read_file(int fd, int length, char* buffer) {
    int total = 0, num_read;
    while (total < length) {
        num_read = read(fd, buffer+total, CHUNK_SIZE);
        if (num_read < 0) {
            perror("Error with read");
            exit(-1);
        }
        if (num_read == 0) { //eof
            return total;
        }
        total+=num_read;
    }
    return total;
}
int main(int argc, char *argv[]) {
    if (argc != 4) {

    }
    // create a TCP connection 
    char* hostname = argv[1];
    unsigned short port = (unsigned short) atoi(argv[2]);
    unsigned int length = atoi(argv[3]);
    int sockfd;
    unsigned total_read = 0;
    char* byte_generator = RANDON_GENERATOR;
    struct sockaddr_in server_addr;

	memset(&server_addr, '0', sizeof(server_addr));
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port); // for endiannes
    // covert internet dot address to network address
    int convert_addr = hostname_to_ip(hostname, &server_addr.sin_addr);
    if (convert_addr) {
        //there was a problem with the hostname to ip

    }
    // create new listen socket
    if (sockfd = socket(AF_INET, SOCK_STREAM, 0) < 0){
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    // connect
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }

    // read from file
    int fd = open(byte_generator, O_RDONLY);
    if (fd < 0) {
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    // read in 
    char* buffer = (char *) calloc(length, sizeof(char));
    if (!buffer) {
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    total_read = read_file(fd, length, buffer);
    if (total_read != length) {
        printf("\n Error: total read is diff from length %s", strerror(errno));
        exit(-1);
    }
    // close
    if (close(fd) < 0) {
        printf("\n Error: %s", sterror(errno));
        exit(-1);
    }

    // now - write to server :) 
    unsigned long header_msg_len = sizeof(unsigned int);
    unsigned long header_sent = 0;
    unsigned int header_length = length;
    while (header_sent < header_msg_len) {
        int bytes_sent = write(sockfd, &header_length + header_sent, header_msg_len - header_sent);
        if (bytes_sent < 0) {
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        header_sent += bytes_sent;
    }
    printf("DONE WRITING HEADER");
    free (buffer);
    // read from server
    unsigned cnt_printable;
    unsigned incoming_message = sizeof(unsigned);
    unsigned total_read;
    while (total_read < incoming_message) {
        int bytes_read = read(sockfd,&cnt_printable + total_read, incoming_message - total_read);
        if (bytes_read < 0){
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        total_read += bytes_read;
    }
    printf("# of printable characters: %u\n", cnt_printable);
    // close socket
    if (close(sockfd) < 0){
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    return 0;

}