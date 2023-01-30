/*
*Author: Sachin Mathad
*Assignment 2 
*Description: Open and write to file arguments given in through CLI 
*/
#include<stdio.h>
#include<syslog.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>

void usage_print(void);

#define FAIL -1

int main(int argc, char ** argv) {
    
    //open log to use syslog
    openlog(NULL, 0, LOG_USER);
    
    // var for fide descriptor
    int fd; 
    // var for write operation
    ssize_t write_status;
    
    // check if the arguments are 3 including the exe name
    if(argc != 3) {
        usage_print();
        syslog(LOG_ERR, "Usage: ./writer <filepath to write to> <string to write into file>\n");
        return 1;
    }

    // open file in Write only, create if not present, if file created, RD and WR permission for all user types
    fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, S_IWOTH | S_IWUSR | S_IWGRP | S_IROTH | S_IRUSR | S_IRGRP);
    // handle file open fail
    if(fd == FAIL) {
       syslog(LOG_ERR, "Error %d: File %s opening error\n", errno, argv[1]); 
    }
    // Log writing to file 
    syslog(LOG_DEBUG, "Writing %s to %s \n", argv[2], argv[1]);
    write_status = write(fd, argv[2], strlen(argv[2]));
    
    // check for fail write 
    if(write_status == FAIL) {
        syslog(LOG_ERR, "Error %d:  Writing to file %s\n", errno, argv[1]);
        return 1;
    }
    
    // check for incomplete write
    if(write_status != strlen(argv[2])) {
        syslog(LOG_ERR, "Error Incomplete: Write corrrupt in file %s\n", argv[1]);
        return 1;
    }

    // close log 
    closelog();
    // close file 
    close(fd);

    return EXIT_SUCCESS;
}

void usage_print() {
    printf("Usage: ./writer <filepath to write to> <string to write into file>\n");
}