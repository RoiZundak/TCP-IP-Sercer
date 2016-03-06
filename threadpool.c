/*
Rotem Saado 201651627
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include "threadpool.h"

//------------------------------------------------------------------------------

//initialize the thread pool struct
threadpool* create_threadpool(int num_threads_in_pool)
{
	threadpool *th_pool;
	int i;
	
	//check if the "num_threads" input is valid
	if(num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
	{
		fprintf(stderr, "ERROR: invalid num of thread\n");
		return NULL;
	}
	//allocate memory for thread pool struct
	th_pool = (threadpool*)malloc(sizeof(threadpool));
	if(!th_pool)
	{
		perror("allocated memory");
		return NULL;
	}
	
	//allocate memory for threads array
	th_pool->threads = (pthread_t*) malloc (sizeof(pthread_t) * num_threads_in_pool);
	if(!th_pool->threads)
	{
		perror("allocated memory");
		free(th_pool);
		return NULL;
	}
	
	//initialize the mutex variable inside the struct
	if(pthread_mutex_init(&th_pool->qlock, NULL) != 0) 
	{
		fprintf(stderr, "ERROR: pthread mutex initialization failed\n");
		free(th_pool->threads);
		free(th_pool);
		return NULL;
	}
	//initialize the condition variable inside the struct
	if(pthread_cond_init(&th_pool->q_not_empty, NULL) != 0) 
	{
		fprintf(stderr, "ERROR: pthread conditional initialization failed\n");
		free(th_pool->threads);
		pthread_mutex_destroy(&th_pool->qlock); 
		free(th_pool);
		return NULL;
	}
	//initialize the condition variable inside the struct
	if(pthread_cond_init(&th_pool->q_empty, NULL) != 0) 
	{
		fprintf(stderr, "ERROR: pthread conditional initialization failed\n");
		free(th_pool->threads);
		pthread_mutex_destroy(&th_pool->qlock);
		pthread_cond_destroy(&th_pool->q_not_empty);
		free(th_pool);
		return NULL;
	}
	
	//initialize all the other members inside the struct
	th_pool->num_threads = num_threads_in_pool;
	th_pool->qsize = 0;
	th_pool->qhead = th_pool->qtail = NULL;
	th_pool->shutdown = 0;
	th_pool->dont_accept = 0; 
	
	//create the threads, the thread init function is do_work and its 
	//argument is the initialized threadpool
	for (i = 0;i < num_threads_in_pool;i++) 
	{
		if(pthread_create(&(th_pool->threads[i]), NULL, do_work, th_pool) != 0)
		{
			fprintf(stderr, "ERROR: pthread create failed\n");
			free(th_pool->threads);
			pthread_mutex_destroy(&th_pool->qlock);
			pthread_cond_destroy(&th_pool->q_not_empty);
			free(th_pool);
			return NULL;		
		}
	}

  	//finally return this init struct
	return th_pool;
}

//------------------------------------------------------------------------------

//method to enter a "job" of type work_t into the queue
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
	//check if threadpool or function exist
	if(!from_me || !dispatch_to_here)
	{
		fprintf(stderr, "ERROR: threadpool or function is NULL \n");
		return;
	}

		
	//allocate memory for work_t
	work_t *currWork = (work_t*)malloc(sizeof(work_t));
	if(!currWork)
	{
		perror("allocated memory");
		return;
	}
	
	//initialize work_t struct
	currWork->routine = dispatch_to_here;
	currWork->arg = arg;
	currWork->next = NULL;
	
	//lock other threads from doing this code
	if(pthread_mutex_lock(&(from_me->qlock)) != 0)
	{
		fprintf(stderr, "ERROR: pthread mutex lock failed\n");
		free(currWork);
		return;
	}
	
	//check if dont accept new items is turn on
	if(from_me->dont_accept) 
	{
		if(pthread_mutex_unlock(&(from_me->qlock)) != 0)
			fprintf(stderr, "ERROR: pthread mutex unlock failed\n");

		free(currWork);  
		return;
	}
	//if we want add this work_t item to queue and queue is empty
	if(from_me->qsize == 0) 
	{
		from_me->qhead = from_me->qtail = currWork;
		from_me->qsize++;
		//signal to other sleeping threads that queue is not empty
		if(pthread_cond_signal(&(from_me->q_not_empty)) != 0)
		{
			fprintf(stderr, "ERROR: pthread conditional signal failed\n");
			return;
		}
		
	}
	//if we want add this work_t item to queue and queue is not empty
	else 
	{
		from_me->qtail->next = currWork;
		from_me->qtail = currWork;
		from_me->qsize++;			
	}
	
	//finally unlock mutex
	if(pthread_mutex_unlock(&(from_me->qlock)) != 0)
	{
		fprintf(stderr, "ERROR: pthread mutex unlock failed\n");
		return;
	}	
}

//------------------------------------------------------------------------------

// The work function of the thread
void* do_work(void* p)
{
	//check if argument p exist
	if(!p)
		return NULL;
		
	threadpool *th_pool = (threadpool*)p;
	work_t* currWork;
	
	while(1)
	{
		
		//lock the critical code
		if(pthread_mutex_lock(&(th_pool->qlock)) != 0)
		{
			fprintf(stderr, "ERROR: pthread mutex lock failed\n");
			return NULL;
		}
		while(th_pool->qsize == 0)
		{
			//check to see if in shutdown mode.
			if(th_pool->shutdown) 
			{
				if(pthread_mutex_unlock(&(th_pool->qlock)) != 0)
					fprintf(stderr, "ERROR: pthread mutex unlock failed\n");

				return NULL;	
			}
			
			//wait until the condition says its no emtpy
			if(pthread_cond_wait(&(th_pool->q_not_empty),&(th_pool->qlock)) != 0)
			{
				fprintf(stderr, "ERROR: pthread cinditional wait failed\n");
				return NULL;
			}
			//check again to see if in shutdown mode.
			if(th_pool->shutdown) 
			{
				if(pthread_mutex_unlock(&(th_pool->qlock)) != 0)
					fprintf(stderr, "ERROR: pthread mutex unlock failed\n");
				return NULL;
			}
				
		}
		
		//get the job
		currWork = th_pool->qhead;
		th_pool->qsize--;  
		
		//reorder hte queue
		if(th_pool->qsize == 0) 
		{
			th_pool->qhead = th_pool->qtail = NULL;
		}
		else if(th_pool->qsize > 0){
			th_pool->qhead = currWork->next;
		}
	
		//if the queue became empty so signal that queue is empty
		if(th_pool->qsize == 0 && th_pool->dont_accept) 
		{
			if(pthread_cond_signal(&(th_pool->q_empty)) != 0)
			{
				fprintf(stderr, "ERROR: pthread conditional signal failed\n");
				free(currWork);
				return NULL;
			}
		}
		//unlock mutex
		if(pthread_mutex_unlock(&(th_pool->qlock)) != 0)
		{
			fprintf(stderr, "ERROR: pthread mutex unlock failed\n");
			free(currWork);
			return NULL;
		}
		
		//finally excute the function and free the work_t struct
		currWork->routine(currWork->arg);
		free(currWork);
	}

}

//------------------------------------------------------------------------------

/*method to kills the threadpool, causing
  all threads in it to commit suicide, and then
  frees all the memory associated with the threadpool*/
void destroy_threadpool(threadpool* destroyme)
{
	//check if destroyme exist
	if(!destroyme)
		return;
		
	int i;
	void* retval;
	
	//unlock this code from other threads
	if(pthread_mutex_lock(&(destroyme->qlock)) != 0)
	{
		fprintf(stderr, "ERROR: pthread mutex lock failed\n");
		return;
	}	
	
	//set dont accept flag to 1	
	destroyme->dont_accept = 1;
	
	while(destroyme->qsize > 0)
	{
		//wait for queue to become empty	
		if(pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock)) != 0)
		{
			fprintf(stderr, "ERROR: pthread conditional wait failed\n");
			return;
		} 

	}
	
	//set shutdown flag to 1
	destroyme->shutdown = 1;
	
	//Signal threads that wait on empty queue, so they can wake up, see shutdown flag and exit.
	if(pthread_cond_broadcast(&(destroyme->q_not_empty)) != 0)
	{
		fprintf(stderr, "ERROR: pthread conditional broadcast failed\n");
		return;
	}

	if(pthread_mutex_unlock(&(destroyme->qlock)) != 0)
	{
		fprintf(stderr, "ERROR: pthread mutex unlock failed\n");
		return;
	}

	for(i = 0; i < destroyme->num_threads ; i++)
	{
		pthread_join(destroyme->threads[i], &retval);
	}
	
	//destroy the mutex and conditions
	if(pthread_mutex_destroy(&(destroyme->qlock)) != 0)
	{
		fprintf(stderr, "ERROR: pthread mutex destroy failed\n");
		return;
	}
	if(pthread_cond_destroy(&(destroyme->q_empty)) != 0)
	{
		fprintf(stderr, "ERROR: pthread conditional destroy failed\n");
		return;
	}
	if(pthread_cond_destroy(&(destroyme->q_not_empty)) != 0)
	{
		fprintf(stderr, "ERROR: pthread conditional destroy failed\n");
		return;
	}
	
	//free threads array and finally the threadpool struct itself
	free(destroyme->threads);
	free(destroyme);
}







