// File: aesdsocket.c
// Description: Socket based program, receives data over connection and appends to file vat/tmp/aesdsocketdata. 
// Author: Sachin Mathad
// Date: 2/15/2023
// ref: Lecture videos, Linux System programming and beej.us

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
#include<linux/fs.h>


#define INIT_BUFFER_SIZE 1024

// socket descriptor 
int sfd; 
// accept socket descriptor 
int client_fd = -1;
// file descriptor 
int fd;

char *store_buffer = NULL;

// reentrant function
void handler(int sig, siginfo_t *info, void *ucontext) {

    if((sig == SIGINT) || (sig == SIGTERM)) {
    
        syslog(LOG_DEBUG, "Caught Signal, Exiting\n");
        closelog();
        close(client_fd);
        close(sfd);
        close(fd);
        closelog();
        unlink("/var/tmp/aesdsocketdata");
        _exit(EXIT_SUCCESS);
    }  
    else {
        syslog(LOG_ERR, "Unknown sig, not supposed to be here\n");
    }

}

void signal_init() {

    struct sigaction action = {0};

    action.sa_flags = SA_SIGINFO; 
    action.sa_sigaction = &handler;
    
    if(sigaction(SIGINT, &action, NULL)) { // non zero return is error
        perror("Signal init error\n");
    }

    if(sigaction(SIGTERM, &action, NULL)) { // non zero return is error
        perror("Signal init error\n");
    }

}

// ref: Linux system programming 
int demonize() {
    pid_t pid = fork(); 
    
    if(pid == -1) {
        perror("Fork error: ");
        return -1;
    }

    //end parent 
    if(pid != 0) {
        exit(EXIT_SUCCESS);
    }

    // daemon/child new session and process group
    if(setsid() == -1) {
        perror("Setsid error: ");
        return -1;
    }

    if(chdir("/") == -1) {
        perror("Chdir error: ");
        return -1;
    }


    //redirect fd 
    open("/dev/null", O_RDWR); 
    dup(0);
    dup(0);

    return 0;
}

int main(int argc, char * argv[]) {

    bool daemon = false; 

    openlog(NULL, LOG_CONS | LOG_PID, 0);


    // check for arguments 
    if(argc > 1) {

        if(strcmp(argv[1], "-d") == 0) {
            daemon = true; 
        }
        else {
        //    printf("FInvalid argument\n");    
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
     //   printf("geraddrinfo error: \n");  
        perror("geraddrinfo error: ");
        return -1;
    }

    // create socket 
    if((sfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
      //  printf("Socket system call error:\n"); 
        perror("Socket system call error: ");
        return -1;
    }

    // set socket to reuse port 
    if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
     //   printf("sockopt error \n"); 
        perror("setsockopt: ");
        return -1;
    } 

    // bind to port 
    if(bind(sfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {//sizeof(struct addrinfo)) == -1) {
     //   printf("bind error :\n"); 
        perror("Bind system call error: ");
        return -1;
    }

    //free servinfo 
    freeaddrinfo(servinfo);
    
    // create daemon if required, bind is complete
    if(daemon) {
        demonize();
    }


    // open file 
    if((fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH)) == -1) {
       // printf("File open error: ");    
        perror("File open error: ");
        return -1;
    }
    // listen socket descriptor and set backlog limit to 20
    if(listen(sfd, 20) == -1) {
     //   printf("listen error:\n");
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
    char *new_ptr = NULL;


    while(1) { // run until signal is received

        
        if((client_fd = accept(sfd, &client_addr, &client_addr_size)) == -1) { // do nothing
        } else { // recv data 
            
             // open file 
            store_buffer = (char *) calloc(INIT_BUFFER_SIZE, sizeof(char));
            if(store_buffer == NULL) {
               // printf("Calloc:\n");
                perror("Calloc fail :");
                return -1;
            }
            //printf("calloc done\n");
            memset(store_buffer, '\0', INIT_BUFFER_SIZE);

            total_length = 0;
            
            struct sockaddr_in *temp_client_addr = (struct sockaddr_in*) &client_addr;    
            // log connected to client 
           // printf("Connected to client %s \n", inet_ntoa(temp_client_addr->sin_addr));
            syslog(LOG_DEBUG, "Connected to client %s", inet_ntoa(temp_client_addr->sin_addr));
            
            // loop until client closes connection or \n is found
            while(((client_rx = recv(client_fd, rx_buffer, sizeof(rx_buffer), 0)) > 0) && (!(eol_found))) {
                
               // printf("received data %s\n", rx_buffer);
                line_length = 0;

                for(int i = 0; i < client_rx; i++) {
                    line_length++;
                    if(rx_buffer[i] == '\n') {
                        eol_found = true;
                     //   printf("eof found at %d\n", line_length);
                        break;
                    }
                    
                }

                total_length += line_length;

                if(total_length >= INIT_BUFFER_SIZE) { // if total store buffer is falling short, realloc

                    if(!(new_ptr = realloc(store_buffer, INIT_BUFFER_SIZE + total_length + 1))) {
                     //   printf("realloc :\n");    
                        perror("Realloc fail :");
                        return -1;
                    } else { // append to the string
                        store_buffer = new_ptr;
                        strncat(store_buffer, rx_buffer, line_length);
                      //  printf("%s concat\n", store_buffer);
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
            if((total_file_length = lseek(fd, 0, SEEK_END))== -1) {
                perror("lseek error: ");
                return -1;

            }
            if(write(fd, store_buffer, total_length) == -1) {
                perror("File writer fail: ");
                return -1;
            }
            
            free(store_buffer);

           // long total_file_length = ftell(fd); 
            if((total_file_length = lseek(fd, 0, SEEK_END))== -1) {
                perror("lseek error: ");
                return -1;

            }
            
            char *send_buffer = (char *) malloc(sizeof(char) * total_file_length);
            if(send_buffer == NULL) {
                perror("Send Malloc fail: ");
                return -1;
            }

            if((lseek(fd, 0, SEEK_SET))== -1) {
                perror("lseek error: ");
                return -1;

            }

            if(read(fd, send_buffer, total_file_length) == -1) {
                perror("Read error: ");
                return -1;
            }
         
            if(send(client_fd, send_buffer, total_file_length, 0) == -1) {
               // printf("send error :\n");
                perror("Send error: ");
                return -1;
            }

            free(send_buffer);
            // after send 
            close(client_fd);
            client_fd = -1;
            eol_found = false;
        }

    }

    close(sfd);
    close(fd);

}


