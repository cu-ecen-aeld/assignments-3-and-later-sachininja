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
#include<pthread.h>
#include<sys/time.h>
#include "../aesd-char-driver/aesd_ioctl.h"


#define AESD_CHAR_DRIVER 1 
#define INIT_BUFFER_SIZE 1024

// socket descriptor 
int sfd; 
// file descriptor 
int fd;

// mutex 
#ifndef AESD_CHAR_DRIVER
pthread_mutex_t mutex;
pthread_t timer_thread;
#endif

bool terminate = false;

typedef struct{
    pthread_t pthread;
    int client_fd; 
    struct sockaddr_in *c_addr;
    bool t_complete; 
}client_meta_t;

typedef struct cnode{
    
    client_meta_t client_meta;
    struct cnode *next;
    
}cnode_t;

cnode_t *head = NULL;
cnode_t *previous = NULL; 
cnode_t *current = NULL;

#ifndef AESD_CHAR_DRIVER
void* timer_handle(void *arg) {

    while(!terminate) {
        time_t now = time(NULL);
        struct tm *local_time = localtime(&now);

        char timestr[100];

        strftime(timestr, sizeof(timestr), "timestamp:%Y-%m-%d %H:%M:%S\n", local_time);

        if(pthread_mutex_lock(&mutex) != 0) {
            perror("Mutex unlock fail:");
        }

        if(write(fd, timestr, strlen(timestr)) == -1) {
            perror("Time File writer fail: ");
        }

        if(pthread_mutex_unlock(&mutex) != 0) {
            perror("Mutex unlock fail:");
        }
        if(!terminate) 
            sleep(10);
        else  
            return NULL;
    }
    return NULL;

}
#endif
// reentrant function
void handler(int sig, siginfo_t *info, void *ucontext) {

    if((sig == SIGINT) || (sig == SIGTERM)) {
    
        terminate = true;
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

void exit_cleaner() {

#ifndef AESD_CHAR_DRIVER
    pthread_mutex_destroy(&mutex);
#endif

    syslog(LOG_DEBUG, "Caught Signal, Exiting\n");
        
    if(sfd > 0)
        close(sfd);
       
    if(fd > 0)
        close(fd);
    
    closelog();

#ifndef AESD_CHAR_DRIVER   
    unlink("/var/tmp/aesdsocketdata");
#endif
}
// if true loop until head == NULL
void check_thread_join(bool val) {

   do{ 
        current = head;
        previous = head;
        while(current != NULL) {
            //Updating head if first node is complete
            if (current->client_meta.t_complete == true) {
                if(current == head) { 
                    head = current->next;
                    current->next = NULL;
                    pthread_join(current->client_meta.pthread, NULL);
                    free(current);
                    current = head; 
                } else {
                    previous->next = current->next;
                    current->next = NULL;
                    pthread_join(current->client_meta.pthread, NULL);
                    free(current);
                    current = previous->next;
                }
            }
            else {
                previous = current;
                current = current->next;
            }
        }

        if(head == NULL) { 
            val = false;
            current = NULL;
        }
    }while(val == true);

}

void* client_handle(void *client_meta) {
    
   // struct sockaddr client_addr; 
   // socklen_t client_addr_size = sizeof(client_addr);
    char *store_buffer = NULL;
    int client_rx = 0; 
    char rx_buffer[INIT_BUFFER_SIZE];
    bool eol_found = false;
    int line_length = 0;
    int total_length = 0;
    char *new_ptr = NULL;
    char ioctl_cmd[] = "AESDCHAR_IOCSEEKTO:";
    struct aesd_seekto ioctl_cmd_request;
    

    client_meta_t *client_meta_copy = (client_meta_t *)client_meta;

    int client_fd = client_meta_copy->client_fd;


    store_buffer = (char *) calloc(INIT_BUFFER_SIZE, sizeof(char));
    if(store_buffer == NULL) {
        perror("Calloc fail :");
        goto error_jump;
    }

    //printf("calloc done\n");
    memset(store_buffer, '\0', INIT_BUFFER_SIZE);
    total_length = 0;
    

    // log connected to client 
    syslog(LOG_DEBUG, "Connected to client %s", inet_ntoa(client_meta_copy->c_addr->sin_addr));
    
    // loop until client closes connection or \n is found
    while(((client_rx = recv(client_fd, rx_buffer, sizeof(rx_buffer), 0)) > 0) && (!(eol_found))) {
        
       // printf("received data %s\n", rx_buffer);
        line_length = 0;
        for(int i = 0; i < client_rx; i++) {
            line_length++;
            if(rx_buffer[i] == '\n') {
                eol_found = true;
                break;
            }
            
        }
        total_length += line_length;
        if(total_length >= INIT_BUFFER_SIZE) { // if total store buffer is falling short, realloc
            if(!(new_ptr = realloc(store_buffer, INIT_BUFFER_SIZE + total_length + 1))) {   
                perror("Realloc fail :");
                // free(store_buffer);
                goto error_jump;

            } else { // append to the string
                store_buffer = new_ptr;
                strncat(store_buffer, rx_buffer, line_length);
            }
        
        } else { // eol found, leave recv while and append to string 
            strncat(store_buffer, rx_buffer, line_length);
        }
        if(eol_found) {
            break;
        }
        
    }
    
    char send_buffer[1024];

    uint32_t bytes_read = 0;

#ifndef AESD_CHAR_DRIVER
    if(pthread_mutex_lock(&mutex) != 0) {
        perror("Mutex lock fail:");
        // free(store_buffer);
        goto error_jump;
    }
#endif

    // check if store buffer received a ioctl handle command 
    // if received, call ioctl 
    if(!(strncmp(store_buffer, ioctl_cmd, strlen(ioctl_cmd)))) {
        //ref: https://www.educative.io/answers/how-to-read-data-using-sscanf-in-c
        // store values of write cmd and offset
        sscanf(store_buffer, "AESDCHAR_IOCSEEKTO:%d,%d", &ioctl_cmd_request.write_cmd, &ioctl_cmd_request.write_cmd_offset);
        if(ioctl(fd, AESDCHAR_IOCSEEKTO, &ioctl_cmd_request)) {
            perror("ioctl error: ");
            goto mutex_unlock_error_jump;
        }
    } else { 
    // else write to file
        if(write(fd, store_buffer, total_length) == -1) {
            perror("File writer fail: ");
            // free(store_buffer);
            goto mutex_unlock_error_jump;

        }  
    } 
#ifndef AESD_CHAR_DRIVER 
    if((lseek(fd, 0, SEEK_SET))== -1) {
        perror("lseek error: ");
        goto mutex_unlock_error_jump;

    }
#endif
        
    while((bytes_read = read(fd, send_buffer, INIT_BUFFER_SIZE)) > 0) {
        if( bytes_read == -1) { 
            perror("Read error: ");
            goto mutex_unlock_error_jump;
        }
        if(send(client_fd, send_buffer, bytes_read, 0) == -1) {
            perror("Send error: ");
            goto mutex_unlock_error_jump;
        }
    }
    
mutex_unlock_error_jump:

    client_meta_copy->t_complete = true;
#ifndef AESD_CHAR_DRIVER
    if(pthread_mutex_unlock(&mutex) != 0) {
        perror("Mutex unlock fail:");
    }
#endif  
error_jump:

    close(client_fd);
    client_fd = -1;
    eol_found = false;
    free(store_buffer);
    return NULL;

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

    // make sokcet non blocking 
    int flags = fcntl(sfd, F_GETFL, 0);
    fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

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

    freeaddrinfo(servinfo);
    
    // create daemon if required, bind is complete
    if(daemon) {
        demonize();
    }


    // open file 

#ifndef AESD_CHAR_DRIVER
    char *filename = "/var/tmp/aesdsocketdata";
    if((fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH)) == -1) {
       // printf("File open error: ");    
        perror("File open error: ");
        return -1;
    }
#else
    char *filename = "/dev/aesdchar";
    if((fd = open(filename, O_RDWR | O_CREAT | O_APPEND, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH)) == -1) {
       // printf("File open error: ");    
        perror("File open error: ");
        return -1;
    }
#endif
    
    // listen socket descriptor and set backlog limit to 20
    if(listen(sfd, 20) == -1) {
     //   printf("listen error:\n");
        perror("Listen error: ");
        return -1;
    }

#ifndef AESD_CHAR_DRIVER
    pthread_mutex_init(&mutex, NULL);
#endif

#ifndef AESD_CHAR_DRIVER
    // create thread for timer 
    int stat = pthread_create(&timer_thread, NULL, timer_handle, NULL);
    if(stat != 0) {
        perror("Thread create: ");
        return -1;
    } 
#endif
    struct sockaddr client_addr; 
    socklen_t client_addr_size = sizeof(client_addr);
    int client_fd = -1;

    while(terminate == false) { // run until signal is received

        
        if(((client_fd = accept(sfd, &client_addr, &client_addr_size)) == -1) && errno == EWOULDBLOCK) { 
          //  printf("no accept, checking for thread join\n");
            check_thread_join(false);
        } else if (client_fd == -1) {// check if threads have completed  
            perror("accept error");
            goto error_clean;

        } else { // recv data 
            //creat a node
            cnode_t *new = (cnode_t *) malloc(sizeof(cnode_t)); 
            if(new == NULL) {
                perror(" Node malloc fail:"); 
                //return -1;
                goto error_clean;
            }
            
            new->client_meta.client_fd = client_fd;
            new->client_meta.c_addr = (struct sockaddr_in *)&client_addr;
            new->client_meta.t_complete = false;
            new->next = NULL;

            //create thread
            int stat = pthread_create(&(new->client_meta.pthread), NULL, client_handle, &(new->client_meta));
            if(stat != 0) {
                perror("Thread create: ");
                free(new);
                goto error_clean;
            } 

            printf(" Thread created ID = %lu\n", new->client_meta.pthread);

            // add node to Linked list, add at the head
            new->next = head;
            head = new;
        }
            

    }
error_clean:
    check_thread_join(true);
    exit_cleaner();
}


