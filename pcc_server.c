#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#define NUM_PRINTABLE 95

int listen_socket;
unsigned pcc_total[NUM_PRINTABLE];
int total_pcc_chars;
int running_threads;
pthread_cond_t cv;
pthread_mutex_t my_mutex;
// handling 
void signal_handler(int signum) {
    int rc = 0;
    if (close(listen_socket) < 0) {
        printf("Error: %s \n", strerror(errno));
        exit(-1);
    }
    if ((rc = pthread_cond_init(&cv, NULL)) != 0) {
        printf("Error: init pthread cond \n %s", strerror(rc));
        pthread_exit(NULL);
    }
    rc = pthread_mutex_lock(&my_mutex);
    if (rc != 0) {
        printf("Error in pthread_mutex_lock: %s \n", atrerror(rc));
        pthread_exit(NULL);
    }
    // wait to all threads to finish
    while (running_threads > 0) {
        rc = pthread_cond_wait(&cv, &my_mutex);
        if (rc) {
            printf("Error in pthread cond wait %s\n", strerror(rc));
            pthread_exit(NULL);
        }
    }
    rc = pthread_mutex_unlock(&my_mutex);
    if (rc != 0) {
        printf("Error in pthread mutex unlocked %s\n", strerror(rc));
        pthread_exit(NULL);
    }
    int i=0;
    for (i=0; i < NUM_PRINTABLE; i++) {
        printf("char '%c' : %u times", i+32, pcc_total[i]);
    }
    rc = pthread_cond_destroy(&my_mutex);
    if (rc) {
        printf("Error in pthread mutex destroy %s\n", strerror(rc));
        pthread_exit(NULL);
    }
    pthread_exit(NULL);
}

void* thread_work(void* p_client_sock) {
    int rc = pthread_mutex_lock(&my_mutex);
    if (rc != 0){
        printf("Error in mutex lock %s\n", strerror(rc));
        pthread_exit(NULL);
    }
    ++running_threads;
    rc = pthread_mutex_unlock(&my_mutex);
    if (rc != 0){
        printf("Error in mutex unlock %s\n", strerror(rc));
        pthread_exit(NULL);
    }
    // init this threads pcc_total
    int this_pcc_total[NUM_PRINTABLE];
    memset(this_pcc_total, 0 , NUM_PRINTABLE);
    int client_fd = (int) p_client_sock;
    // first read the header (length)
    unsigned cnt;
    unsigned incoming_header = sizeof(unsigned);
    unsigned total_read;
    while (total_read < incoming_header) {
        int bytes_read = read(client_fd,&cnt + total_read, incoming_header - total_read);
        if (bytes_read < 0){
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        total_read += bytes_read;
    }
    // allocate buffer
    char * msg = (char *) calloc((size_t) cnt, sizeof(char));
    if (!msg) {
        printf("Error: calloc msg failed %s\n", strerror(errno));
        exit(-1);
    }
    int total_read = 0;
    int printable_chars =0;
    while (total_read < cnt) {
        int bytes_read = read(client_fd, msg+total_read, cnt-total_read);
        if (bytes_read < 0){
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        total_read += bytes_read;
        int i=0;
        for (i=0; i< bytes_read; i++) {
            if(msg[i] >= 32 && msg[i] <= 126) {
                ++this_pcc_total[msg[i]-32];
                ++printable_chars;
            }
        }
    }
    // write back to client the result
    // int num_written = 0;
    int total_written = 0;
    int num_towrite = sizeof(unsigned);
    while (total_written < num_towrite) {
        int num_written = write(client_fd, &printable_chars+total_written, num_towrite-total_written);
        if (num_written < 0){
            printf("\n Error: %s", strerror(errno));
            exit(-1);
        }
        total_written += num_written;
    }
    // close
    if (close(client_fd) < 0){
        printf("\n Error: %s", strerror(errno));
        exit(-1);
    }
    // update atomically the pcc_toal
    rc = pthread_mutex_lock(&my_mutex);
    if (rc != 0) {
        printf("\n Error: in mutex lock %s", strerror(errno));
        pthread_exit(NULL);
        exit(-1);
    }
    int i;
    for (i = 0; i < NUM_PRINTABLE ; i++){
        pcc_total[i] += this_pcc_total[i];
    }
    total_pcc_chars += printable_chars;
    --running_threads;
    if ((rc = pthread_cond_signal(&cv)) != 0) {
        printf("\n Error: in cond signal %s", strerror(errno));
        pthread_exit(NULL);
        exit(-1);
    }
    rc = pthread_mutex_unlock(&my_mutex);
    if (rc != 0){
        printf("\n Error: in mutex unlock %s", strerror(errno));
        pthread_exit(NULL);
        exit(-1);
    }
    pthread_exit(NULL);
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Error: not enough args\n");
        exit(-1);
    }
    int port_server = atoi(argv[1]);
    // create sigint handler
    int running_threads = 0;
    total_pcc_chars = 0;
    struct sigaction new_interrupt;
    memset(&new_interrupt, 0, sizeof(new_interrupt));
    // assign 
    new_interrupt.sa_handler = signal_handler;
    if (sigaction(SIGINT, &new_interrupt, NULL) != 0){
        printf("Signal handler reg failed %s\n", strerror(errno));
        exit(-1);
    }
    // init the pcc total
    memset(pcc_total,0,NUM_PRINTABLE);
    // init the mutex
    int rc;
    if ((rc = pthread_mutex_init(&my_mutex, NULL)) != 0) {
        printf("Error in mutex init %s \n", strerror(errno));
        exit(-1);
    }

    struct sockaddr_in server_addr;
    listen_socket = -1;
    // create socket
    if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error in socket function %s\n", strerror(errno));
        exit(-1);
    }
    // build server_addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_server);
    // bind
    if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0){
        printf("Error: bind failed %s\n", strerror(errno));
        exit(-1);
    }
    // start listen
    if (listen(listen_socket, 10) != 0) {
        printf("Error: listen function failed %s\n", strerror(errno));
        exit(-1);
    }
    int client_sock;
    while (1) {
        // accept new connection 
        client_sock = accept(listen_socket, NULL, NULL);
        if (client_sock < 0 && errno != EINTR) {
            printf("Error: accpet failed %s\n", strerror(errno));
            exit(-1);
        }
        pthread_t t;
        // create new thread
        int cpthread = pthread_create(&t, NULL, thread_work, (void *)client_sock);
        if (cpthread) {
            printf("\n Error: in pthread_create %s", strerror(errno));
            pthread_exit(NULL);
            exit(-1);
        } 

    }
}