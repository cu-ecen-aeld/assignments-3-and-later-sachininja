#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    bool ret_status = true;

    int retval = system(cmd); 
    
    if(-1 == retval) {
        ret_status = false;
        perror("system() call failure:");
    }

    return ret_status;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int ret_status = true;
    pid_t ret_val = fork();

    // check for errno in fork 
    if(-1 == ret_val) {
        ret_status = false; 
        perror("Fork error: ");
    }
    // parent operation to wait for child to turn zombie
    else if(ret_val > 0) {
        int status; 
        pid_t wait_status = wait(&status);
        if(-1 == wait_status) {
            perror("Wait error: ");
            ret_status = false; 
        }
        else {
            // check if child called _exit to exit normally
            if(WIFEXITED(status)) {
                // check if child process did not exit success(failure == 1)
                if(WEXITSTATUS(status) != 0) {
                   ret_status = false; 
                }
            } else {
                // if child did not call _exit() 
                ret_status = false; 
            }
        }
    }
    // child process
    else if(ret_val == 0) {
        int exec_status = execv(command[0], command);
        if(-1 == exec_status) {
            perror("Exec error: ");
            exit(-1);
        }
    }

    va_end(args);

    return ret_status;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if(-1 == fd) {
        perror(" File open: ");
        exit(-1);
    }
    int ret_status = true;
    pid_t ret_val = fork();

    // check for errno in fork 
    if(-1 == ret_val) {
        ret_status = false; 
        perror("Fork error: ");
    }
    // parent operation to wait for child to turn zombie
    else if(ret_val > 0) {
        int status; 
        pid_t wait_status = wait(&status);
        if(-1 == wait_status) {
            perror("Wait error: ");
            ret_status = false; 
        }
        else {
            // check if child called _exit to exit normally
            if(WIFEXITED(status)) {
                // check if child process did not exit success(failure == 1)
                if(WEXITSTATUS(status) != 0) {
                   ret_status = false; 
                }
            } else {
                // if child did not call _exit() 
                ret_status = false; 
            }
        }
    }
    // child process
    else if(ret_val == 0) {
        // redirection make fd and 1 to be used interchangebly
        if (dup2(fd, 1) < 0) { 
            perror("dup2"); 
            exit(-1); 
        }
        int exec_status = execv(command[0], command);
        if(-1 == exec_status) {
            perror("Exec error: ");
            exit(-1);
        }
    }

    va_end(args);
    close(fd);

    return ret_status;
}
