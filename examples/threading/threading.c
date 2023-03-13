#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...) printf("DEBUG MSG: " msg "\n" , ##__VA_ARGS__)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define MILLI_SLEEP_TO_uSLEEP(x) (x * 1000)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    bool ret_status = true;

    // first sleep 
    if(usleep(MILLI_SLEEP_TO_uSLEEP(thread_func_args->wait_to_obtain_ms))) {
        // non zero value is failure
        ret_status = false;

        ERROR_LOG(" Wait to obtain sleep fail %d", errno);
    
    } else { // SUCCESS! now access mutex 
        
        if(pthread_mutex_lock(thread_func_args->mutex)) {
            // non zero value is fail
            ret_status = false;
            ERROR_LOG(" Mutex acquire fail %d", errno);
        
        } else { // SUCCESS! now sleep... 

            if(usleep(MILLI_SLEEP_TO_uSLEEP(thread_func_args->wait_to_release_ms))) {
                
                // non zero value is failure
                ret_status = false;

                ERROR_LOG(" Wait to release sleep fail %d", errno);
                // unlock mutex, what if it fails? log error?
                
                if(pthread_mutex_unlock(thread_func_args->mutex)) {
                    ERROR_LOG(" Mutex unlock fail %d", errno);
                }
           
            } else { // SUCCESS! now release mutex

                if(pthread_mutex_unlock(thread_func_args->mutex)) {
                    // non zero value is failure
                    ERROR_LOG(" Mutex unlock fail %d", errno);
                    ret_status = false;
                } 

            }
        }
    }


    if(false == ret_status) {
        thread_func_args->thread_complete_success = false;
    } else {
        thread_func_args->thread_complete_success = true;
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    bool bool_ret = true;
    struct thread_data *var_thread_data =  (struct thread_data *) malloc(sizeof(struct thread_data));
    var_thread_data->thread = thread;
    var_thread_data->mutex = mutex;
    var_thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    var_thread_data->wait_to_release_ms = wait_to_release_ms;

    if(pthread_create(thread, NULL, threadfunc, var_thread_data)) {
        // non zero value is failure
        ERROR_LOG("Thread Create error : %d", errno);
        bool_ret = false;
        // free memory since thread was never created and test won't free it as threadID is unavailable? 
        free(var_thread_data);
    } else {

        DEBUG_LOG("Thread create success\n");
    }
    
    
    return bool_ret;
}

