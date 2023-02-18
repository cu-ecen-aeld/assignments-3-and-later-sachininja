// File: aesdsocket.c
// Description: Socket based program, receives data over connection and appends to file vat/tmp/aesdsocketdata. 
// Author: Sachin Mathad
// Date: 2/15/2023
// ref: Lecture videos and beej.us

#include<stdint.h>
#include<stdbool.h>
#include<stdio.h>
#include<syslog.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<signal.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>


#define INIT_BUFFER_SIZE 1024
// global variables for socket and file descriptor 

// socket descriptor 
int sfd; 
// accept socket descriptor 
int client_fd = -1;
// file descriptor 
int fd;
// malloced buffer 
char *client_data = NULL; 

void handler(int sig, siginfo_t *info, void *ucontext) {

    switch(sig) {

        case SIGINT: 
        break;

        case SIGTERM:
        break;
    }

}

void signal_init() {

    struct sigaction action;

    action.sa_flags = SA_SIGINFO; 
    action.sa_sigaction = &handler;
    
    if(sigaction(SIGINT, &action, NULL)) { // non zero return is error
        perror("Signal init error\n");
    }

    if(sigaction(SIGTERM, &action, NULL)) { // non zero return is error
        perror("Signal init error\n");
    }

}

int main(int argc, char * argv[]) {

    bool daemon = false; 

    openlog(NULL, LOG_CONS | LOG_PID, 0);

    // open file 

    if((fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH)) == -1) {

        perror("File open error: ");
        return -1;
    }

    // check for arguments 
    if(argc > 1) {

        if(strcmp(argv[1], "-d") == 0) {
            daemon = true; 
        }
        else {
            printf("Invalid argument\n");
        }

    }

    // init signals 
    signal_init();

    // init scructs
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;  // will point to the results



    // setsckopt 
    const int yes = 1;
    
    memset(&hints, 0, sizeof(hints)); // make sure the struct is empty
    hints.ai_family = AF_INET;     // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    // setup servinfo
    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
        perror("geraddrinfo error: ");
        return -1;
    }

    // create socket 
    if((sfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        perror("Socket system call error: ");
        return -1;
    }

    // set socket to reuse port 
    if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt: ");
        return -1;
    } 

    // bind to port 
    if(bind(sfd, servinfo->ai_addr, sizeof(struct addrinfo)) == -1) {
        perror("Bind system call error: ");
        return -1;
    }

    //free servinfo 
    freeaddrinfo(servinfo);
    
    // create daemon if required


    // listen socket descriptor and set backlog limit to 20
    if(listen(sfd, 20) == -1) {
        perror("Listen error: ");
        return -1;
    }

    struct sockaddr client_addr; 
    socklen_t client_addr_size = sizeof(client_addr);
    int client_rx = 0; 
    char rx_buffer[INIT_BUFFER_SIZE];
    bool eol_found = false;
    int line_length = 0;
    int total_length = 0;

    char *store_buffer = (char *) malloc(sizeof(char)* INIT_BUFFER_SIZE);
    if(store_buffer == NULL) {
        perror("Malloc fail :");
        return -1;
    }

    while(1) { // run until signal is received

        if((client_fd = accept(sfd, &client_addr, &client_addr_size)) == -1) { // do nothing
            
        } else { // recv data 
            
            
            total_length = 0;
            
            struct sockaddr_in *temp_client_addr = (struct sockaddr_in*) &client_addr;    
            // log connected to client 
            syslog(LOG_DEBUG, "Connected to client %s", inet_ntoa(temp_client_addr->sin_addr));
            
            // loop until client closes connection or \n is found
            while(((client_rx = recv(client_fd, rx_buffer, sizeof(rx_buffer), 0)) > 0) && (!(eol_found))) {
                
                for(int i = 0, line_length = 0; i < client_rx; i++) {
                    line_length++;
                    if(rx_buffer[i] == '\n') {
                        eol_found = true;
                        break;
                    }
                    
                }
                
                total_length += line_length;

                if(total_length >= INIT_BUFFER_SIZE) { // if total store buffer is falling short, realloc

                    if(!(realloc(store_buffer, INIT_BUFFER_SIZE + line_length + 1))) {
                            
                        perror("Realloc fail :");
                        return -1;
                    } else { // append to the string
                       //memset();
                        strncat(store_buffer, rx_buffer, line_length);
                    }
                
                } else { // eol found, leave recv while and append to string 
                    strncat(store_buffer, rx_buffer, line_length);
                }

                if(eol_found) {
                    break;
                }
                
            }
            
            
            // seek to the EOF
            off_t total_file_length = 0;
            if((total_file_length = lseek(fd, 0, SEEK_END))== 1) {
                perror("lseek error: ");
                return -1;

            }
            if(write(fd, store_buffer, total_length) == -1) {
                perror("File writer fail: ");
                return -1;
            }
            
            free(store_buffer);

           // long total_file_length = ftell(fd); 
            if((total_file_length = lseek(fd, 0, SEEK_END))== 1) {
                perror("lseek error: ");
                return -1;

            }
            
            char *send_buffer = (char *) malloc(sizeof(char) * total_file_length);
            if(send_buffer == NULL) {
                perror("Send Malloc fail: ");
                return -1;
            }

            if(read(fd, send_buffer, total_file_length) == -1) {
                perror("Read error: ");
                return -1;
            }

            if(send(client_fd, send_buffer, total_file_length, 0) == -1) {
                perror("Send error: ");
                return -1;
            }

            free(send_buffer);
            // after send 
            close(client_fd);
        }

    }

    close(sfd);
    close(fd);

}


