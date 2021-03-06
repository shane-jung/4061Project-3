#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#define MAX_THREADS 100
#define MAX_QUEUE_LEN 100
#define MAX_CE 100
#define INVALID -1
#define BUFF_SIZE 1024
#define MAX_REQUEST_LEN 1024
#define PERMS 0666

/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGGESTION. FEEL FREE TO MODIFY AS NEEDED
*/

// structs:
typedef struct request_queue {
  int fd;
  char *request;
} request_t;

typedef struct cache_entry {
  int freq; 
  int len;
  int index;
  char *request;
  char *content;
  struct cache_entry_t* next; 
} cache_entry_t;


//globals
static volatile sig_atomic_t doneFlag = 0;
request_t queue [MAX_QUEUE_LEN];
static int num_dispatcher = 0;
static int num_workers = 0;
int d_arg [MAX_THREADS];
int w_arg [MAX_THREADS];
int insert_idx = 0;
int retrieve_idx = 0;
int items_in_queue = 0;	//num of items in queue that haven't yet been retrieved by worker
static int queue_length =1;
FILE* logfile;
cache_entry_t** cache; 
int cache_size = 0; 
int c_idx = 0;
int max_cache_size;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;


// moved the section for Extra Credit A farther down.


/* ************************ Cache Code [Extra Credit B, did not implement] **************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  /// return the index if the request is present in the cache
  for(int i = 0; i < cache_size; i++){
    if(!strcmp(cache[i]->request, request)) {
      cache[i]->freq++;
      return i;
    }
  }
  return -1;
}

int getMinFreqInCache(){
  int min = cache[0]->freq;
  int minIndex = 0;
  for(int i = 1; i < max_cache_size; i++){
    if(cache[i]->freq < min){
      min = cache[i]->freq;
      minIndex = i;
    }
  }
  return minIndex;
}

void deleteCacheEntry(cache_entry_t* toDelete){
  free(toDelete->content);
  free(toDelete->request);
}

// Function to add the request and its file content into the cache
// It should add the request at an index according to the cache replacement policy
// Make sure to allocate/free memory when adding or replacing cache entries
void addIntoCache(char *mybuf, char *memory , int memory_size){
  int index; 
  if(cache_size < max_cache_size){  //if cache is not full yet, just add it to the end
    index = c_idx;
    c_idx++;
    cache_size++;
  } else { //otherwise, we try to find the request with min frequency to replace 
    index = getMinFreqInCache();
    deleteCacheEntry(cache[index]);
  }
  cache_entry_t* entry = malloc(sizeof(cache_entry_t));
  entry -> len     = memory_size; 
  entry -> freq    = 1;
  entry -> request = malloc(MAX_REQUEST_LEN);
  entry -> request = mybuf;
  entry -> content = malloc(memory_size);
  entry -> content = memory;
  cache[index] = entry;
  return;
}

// clear the memory allocated to the cache
void deleteCache(){
  int i = 0;
  for(; i < cache_size; i++){
    deleteCacheEntry(cache[i]);
    free(cache[i]);
  }
  for( ;i < max_cache_size; i++){
    free(cache[i]);
  }
  free(cache);
}


// Function to initialize the cache
void initCache(){
  // Allocating memory and initializing the cache array
  cache = (cache_entry_t**) malloc(sizeof(cache_entry_t*) * max_cache_size);
  for(int i = 0; i < max_cache_size; i++){
    cache[i] = (cache_entry_t *) malloc(sizeof(cache_entry_t*));
  }
}

// utility to help visualize cache
void printCache(){
  printf("Cache Size: %d\nCache Max Size: %d\n", cache_size, max_cache_size);
  for(int i = 0; i < cache_size; i++){
    cache_entry_t* curr = cache[i];
    printf("Entry %d | request: %s | freq: %d | len: %d\n", i+1, curr->request, curr->freq, curr-> len);
  }
}
/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char * mybuf) {
  // Should return the content type based on the file type in the request
  // (See Section 5 in Project description for more details)
  const char* extension = strrchr(mybuf, '.'); 
  extension += 1;

  char* content_type = (char*) malloc(BUFF_SIZE*sizeof(char));
  if(!strcmp(extension, "html") || !strcmp(extension, "htm")){
    strcpy(content_type, "text/html");
  } else if (!strcmp(extension, "jpg")){
    strcpy(content_type, "image/jpeg");
  } else if (!strcmp(extension, "gif")){
    strcpy(content_type, "image/gif");
  } else {
    strcpy(content_type, "text/plain");
  }
  return content_type;
}

// Function to open and read the file from the disk into the memory
// Add necessary arguments as needed
int readFromDisk(char* file, char* contents, size_t size) {
  // Open and read the contents of file given the request

  int fd, bytes = 0;
  if( (fd = open(file, O_RDONLY)) == -1){
    printf("File failed to open.\n");
    return -1;
  }

  bytes = read(fd, contents, size);
  return bytes;
}

/**********************************************************************************/

// Function to receive the request from the client and add to the queue

void * dispatch(void *arg) {

  while (1) {
  
    // Accept client connection 

    int fd = accept_connection();
    if(fd < 0){
      continue;
    }

    // Get request from the client
    
    char filename[BUFF_SIZE];
    if(get_request(fd, filename)){
       continue;
    }
    
    // Set lock
    
    if(pthread_mutex_lock(&queue_lock)){
    	printf("Error obtaining lock \n");
    	continue;
    }
    
    // wait until queue is not full
    
    while(items_in_queue == queue_length){
    	if(pthread_cond_wait(&queue_not_full, &queue_lock)){
    		continue;
    	}
    }
    
    items_in_queue++;
    
    if(insert_idx >= queue_length){
    	insert_idx = 0;
    }
    
    // Add the request into the queue
    
    request_t req; 
    req.fd = fd;
    req.request = filename;
    queue[insert_idx] = req;
    insert_idx++;
    
    
    
    //signal that queue is not empty
    
    if(pthread_cond_signal(&queue_not_empty)){
    	continue;
    }
    
    //Unlock
    
    if(pthread_mutex_unlock(&queue_lock)){
    	printf("error releasing lock \n");
    	continue;
    }
    
   }
   return NULL;
}

/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void * arg) {
  int num_requests = 0;
  while (1) {
    sleep(1);

    //set lock
    
    if(pthread_mutex_lock(&queue_lock)){
    	printf("error obtaining lock \n");
    	continue;
    }
    
    
    
    // wait until queue is not empty
    
    while(items_in_queue == 0 || *(int*)arg >= num_workers){
    	// delete thread if mandated by dynamic worker thread pool
    
   	 if(*(int*)arg >= num_workers){
    		//printf("deleting thread %d \n", *(int*)arg);
    		
    		//release lock
    		
    		 if(pthread_mutex_unlock(&queue_lock)){
    			printf("error releasing lock \n");
    			continue;
    		 }

    		//exit thread
    		
    		pthread_exit(0);
    	}
    	if(pthread_cond_wait(&queue_not_empty, &queue_lock)){
    		continue;
    	}
    	
    }
    
    items_in_queue--;
    
    if(retrieve_idx >= queue_length){
    	retrieve_idx = 0;
    }
    
    // Get the request from the queue
    request_t req = queue[retrieve_idx];
    retrieve_idx++;
    
    num_requests++;
    
    // signal that queue is not full
    
    if(pthread_cond_signal(&queue_not_full)){
    	continue;
    }
    
    //unlock
    
    if(pthread_mutex_unlock(&queue_lock)){
    	printf("error releasing lock \n");
    	continue;
    }

    // Get the data from the disk or the cache (extra credit B)

	//set lock
	if(pthread_mutex_lock(&cache_lock)){
		printf("error obtaining lock \n");
		continue;
	}
    
    char* path = getcwd(NULL, BUFF_SIZE);
    strcat(path, req.request);
    int cache_index, numbytes;
    char* contents;
    bool cacheHit = false;
    if((cache_index = getCacheIndex(path)) == -1){
      struct stat st; 
      stat(path, &st);
      contents = malloc(st.st_size);
      numbytes = readFromDisk(path, contents, st.st_size);
      if(max_cache_size > 0){
        addIntoCache(path, contents, numbytes);
      }
    } else{
      cacheHit = true;
      cache_entry_t* data = cache[cache_index];
      numbytes = data -> len;
      contents = malloc(numbytes);
      contents = data -> content;
    }
    
     if(pthread_mutex_unlock(&cache_lock)){
    	printf("error releasing lock \n");
    	continue;
    }
    
    
    //set lock
    if(pthread_mutex_lock(&log_lock)){
    	printf("error obtaining lock \n");
    	continue;
    }

    // Log the request into the file and terminal
    fprintf(logfile, "[%d][%d][%d][%s]", *(int*)arg, num_requests, req.fd, req.request);
    if(numbytes <= 0){
      char* error = malloc(BUFF_SIZE);
      if(return_error(req.fd, error)){
        printf("FAILED RETURNING ERROR\n");
      }
      fprintf(logfile, "[%s]", "Requested file not found.");
    } else {
      fprintf(logfile, "[%d]", numbytes);
      char* content_type = getContentType(req.request);
      return_result(req.fd, content_type, contents, numbytes);
      free(content_type);
    }
    if(cacheHit){
      fprintf(logfile, "[HIT]\n");
    } else {
      fprintf(logfile, "[MISS]\n");
    }
    
    
    //Unlock
    
    if(pthread_mutex_unlock(&log_lock)){
    	printf("error releasing lock \n");
    	continue;
    }

    // return the result

    char* content_type = getContentType(req.request);
   
    return_result(req.fd, content_type, contents, numbytes);
    
    free(content_type);
    
  }
  return NULL;
}


/* ******************** Dynamic Pool Code  [Extra Credit A] **********************/
// Extra Credit: This function implements the policy to change the worker thread pool dynamically
// depending on the number of requests
void * dynamic_pool_size_update(void *arg) {
  while(1) {
    // Run at regular intervals
    sleep(2);
    
    // Increase / decrease dynamically based on your policy
    
    if(items_in_queue > num_workers){
    	printf("creating %d new workers \n", items_in_queue-num_workers);
    	for(int n = num_workers; n < items_in_queue; n++){
    		pthread_t p;
    		w_arg[n] = n;
    		num_workers++;
    		if(pthread_create(&p, NULL, worker, &w_arg[n])){
    			printf("error creating thread \n");
    		}
  	}
    }
    
    else if(items_in_queue + 1 < num_workers){
    	printf("deleting %d workers \n", num_workers - items_in_queue - 1);
    	num_workers = items_in_queue + 1;
    	//printf("items in queue: %d, num workers: %d \n", items_in_queue, num_workers);
    }
    
  }
}
/**********************************************************************************/
/**********************************************************************************/

void gracefulTerminate(int signal){
  doneFlag = 1;
}


int main(int argc, char **argv) {

  // Error check on number of arguments
  if(argc != 8){
    printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
    return -1;
  }

  // Get the input args
  
  int port = atoi(argv[1]);
  char* path = argv[2];
  num_dispatcher = atoi(argv[3]);
  num_workers = atoi(argv[4]);
  bool dynamic_flag = atoi(argv[5]);
  queue_length = atoi(argv[6]);
  max_cache_size = atoi(argv[7]);
  //printf("Port: %d\nPath: %s\nNum dispatchers: %d\nNum workers: %d\nDynamic Flag: %d\nQueue Length: %d\nCache Size: %d\n", port, path, num_dispatcher, num_workers, dynamic_flag, queue_length, cache_size);

  // Perform error checks on the input arguments

  if(port < 1025 || port > 65535) {
    printf("You may only use ports 1025-65535.\n");
    return INVALID;
  } 
  if(queue_length <= 0){
    printf("Queue length must be greater than 0.\n");
    return INVALID; 
  }
  
  if(num_dispatcher > MAX_THREADS || num_dispatcher < 0 || num_workers > MAX_THREADS || num_workers < 0){
  	printf("Number of dispatcher and workers must be between %d and 100. \n", MAX_THREADS);
  	return INVALID;
  }

  // Change SIGINT action for grace termination
  
  struct sigaction act;
  act.sa_handler = gracefulTerminate;
  act.sa_flags = 0;
  if(sigemptyset(&act.sa_mask) == INVALID || sigaction(SIGINT, &act, NULL) == INVALID){
    printf("Failed to set SIGINT handler.\n");
    return INVALID;
  } 

  // Open log file

  logfile = fopen("webserver_log", "w");
  if(logfile == NULL){
    printf("Failed to open web_server_log\n.");
    return INVALID;
  }

  // Change the current working directory to server root directory

  chdir(path);
  // printf("%s\n", getcwd(NULL, 1000));
  
  // Initialize cache (extra credit B)
  initCache();
  

  // Start the server

  init(port);

  // Create dispatcher and worker threads (all threads should be detachable)
  for(int n = 0; n < num_dispatcher; n++){
    pthread_t p;
    d_arg[n] = n;
    // printf("Dispatcher #%d\n", n);
    if(pthread_create(&p, NULL, dispatch, &d_arg[n])){
    	printf("error creating thread \n");
    }
  }
  for(int n = 0; n < num_workers; n++){
    pthread_t p;
    w_arg[n] = n;
    // printf("Worker #%d\n", n);
    if(pthread_create(&p, NULL, worker, &w_arg[n])){
    	printf("error creating thread \n");
    }
  }

  // Create dynamic pool manager thread (extra credit A)

  if(dynamic_flag){
  	//create thread
  	pthread_t p;
  	if(pthread_create(&p, NULL, dynamic_pool_size_update, (void *)MAX_THREADS)){
  		printf("error creating thread \n");
  	}
  }
  

  // Terminate server gracefully
  while(!doneFlag) sleep(1);

  // Print the number of pending requests in the request queue

  printf("Number of pending requests in queue: %d\n", insert_idx - retrieve_idx);

  // close log file

  fclose(logfile);

  // Remove cache (extra credit B)

  deleteCache();


  return 0;
}
