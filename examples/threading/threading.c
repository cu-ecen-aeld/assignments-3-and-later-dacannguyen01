#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success = false;
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    usleep(thread_func_args->m_wait_to_obtain_ms * 1000);
    int ret = pthread_mutex_lock(thread_func_args->m_mutex);
    if(ret != 0)
    {
    	ERROR_LOG("pthread_mutex_lock failed code %d", ret);
    	return thread_param;
    }
    
    usleep(thread_func_args->m_wait_to_release_ms * 1000);
    ret = pthread_mutex_unlock(thread_func_args->m_mutex);
    if(ret != 0)
    {
    	ERROR_LOG("pthread_mutex_unlock failed code %d", ret);
    	return thread_param;
    }
    thread_func_args->thread_complete_success = true;
    
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
     
    struct thread_data* m_thread_data = malloc(sizeof(struct thread_data));
    if(m_thread_data == NULL) {
    	ERROR_LOG("malloc failed");
    	return false;
    }
    
    m_thread_data->m_thread = *thread;
    m_thread_data->m_mutex = mutex;
    m_thread_data->m_wait_to_obtain_ms = wait_to_obtain_ms;
    m_thread_data->m_wait_to_release_ms = wait_to_release_ms;
    
    int ret = pthread_create(thread, NULL, threadfunc, (void *)m_thread_data);
    if (ret != 0)
    {
    	ERROR_LOG("pthread_create failed code %d", ret);
    	m_thread_data->thread_complete_success = false;
    	free(m_thread_data);
    	return false;
    }
    
    return true;
}

