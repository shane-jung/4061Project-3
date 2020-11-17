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
#define MAX_queue_len 100
#define MAX_CE 100
#define INVALID -1
#define BUFF_SIZE 1024
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
request_t queue [MAX_queue_len];
int insert_idx = 0;
int retrieve_idx = 0;
FILE* logfile;
cache_entry_t* cache; 
int cache_size = 0; 
pthread_mutex_t lock; 

/* ******************** Dynamic Pool Code  [Extra Credit A] **********************/
// Extra Credit: This function implements the policy to change the worker thread pool dynamically
// depending on the number of requests
void * dynamic_pool_size_update(void *arg) {
  while(1) {
    // Run at regular intervals
    // Increase / decrease dynamically based on your policy
  }
}
/**********************************************************************************/

/* ************************ Cache Code [Extra Credit B] **************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  /// return the index if the request is present in the cache
  return -1;
}

void deleteCacheEntry(cache_entry_t* toDelete){
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory , int memory_size){
  // It should add the request at an index according to the cache replacement policy
  // Make sure to allocate/free memory when adding or replacing cache entries

  return;
}

// clear the memory allocated to the cache
void deleteCache(){
  // De-allocate/free the cache memory
}


// Function to initialize the cache
void initCache(){
  // Allocating memory and initializing the cache array
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

  int fd, bytes, totalbytes = 0;
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
    //printf("fd: %d\n", fd);
    if(fd < 0){
      continue;
    }

    // Get request from the client
    
    char filename[BUFF_SIZE];
    if(get_request(fd, filename)){
       continue;
    }
    // Add the request into the queue

    request_t req; 
    req.fd = fd;
    req.request = filename;
    queue[insert_idx] = req;
    insert_idx++; 
   }
   return NULL;
}

/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void * arg) {
  int num_requests = 0;
  while (1) {
    sleep(1);

    if(retrieve_idx == insert_idx) {
      //fprintf(logfile, "Handled all requests\n");
      continue;  //if we've handled all requests
    }
    
    // Get the request from the queue

    //set lock
    request_t req = queue[retrieve_idx];
    retrieve_idx++; 
    num_requests++;
    //unlock

    // Get the data from the disk or the cache (extra credit B)

    struct stat st; 
    char* path = getcwd(NULL, BUFF_SIZE);
    strcat(path, req.request);
    stat(path, &st);
    char* contents = malloc(st.st_size);
    int numbytes = readFromDisk(path, contents, st.st_size);

    // Log the request into the file and terminal

    fprintf(logfile, "[%d][%d][%d][%s]", *(int*)arg, num_requests, req.fd, req.request);
    if(numbytes <= 0){
      char* error = malloc(BUFF_SIZE);
      return_error(req.fd, error);
      fprintf(logfile, "[%s]", error);
      fprintf(logfile, "[MISS]\n");
      return 0;
    } else {
      fprintf(logfile, "[%d]", numbytes);
    }
    fprintf(logfile, "[MISS]\n");

    // return the result

    char* content_type = getContentType(req.request);
    return_result(req.fd, content_type, contents, numbytes);
    free(content_type);
    
  }
  return NULL;
}
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
  int num_dispatcher = atoi(argv[3]);
  int num_workers = atoi(argv[4]);
  bool dynamic_flag = atoi(argv[5]);
  int queue_length = atoi(argv[6]);
  int cache_size = atoi(argv[7]);
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
  cache = NULL;
  

  // Start the server

  init(port);

  // Create dispatcher and worker threads (all threads should be detachable)
  int d_arg[MAX_THREADS];
  for(int n = 0; n < num_dispatcher; n++){
    pthread_t p;
    d_arg[n] = n;
    // printf("Dispatcher #%d\n", n);
    pthread_create(&p, NULL, dispatch, &d_arg[n]);
  }
  int w_arg[MAX_THREADS];
  for(int n = 0; n < num_workers; n++){
    pthread_t p;
    w_arg[n] = n;
    // printf("Worker #%d\n", n);
    pthread_create(&p, NULL, worker, &w_arg[n]);
  }

  // Create dynamic pool manager thread (extra credit A)

  if(dynamic_flag);

  // Terminate server gracefully
  while(!doneFlag) sleep(1);

  // Print the number of pending requests in the request queue

  printf("\nNumber of pending requests in queue: %d\n", insert_idx - retrieve_idx);

  // close log file

  fclose(logfile);

  // Remove cache (extra credit B)


  return 0;
}
