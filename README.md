/* Test machine: CSELAB_machine_name
* Name: Kieran Arora, Shane Jung, Clara Lemaitre
* X500: arora119, lemai014, jung0237 */

How to compile and run program: 
Use make to compile. 
Run with ./web_server port path num_dispatch num_workers dynamic_flag qlen cache_entries 
Test with wget in another terminal, for example: wget http://127.0.0.1:9000/image/jpg/29.jpg

How program works: 
The server program creates dispatcher and worker threads. The dispatcher threads read requests from the client and place them into the queue. The worker threads read the requests and serve them for the client. The synchronization is accomplished using mutexes and condition variables to ensure that only one thread can access/modify the queue at a time. The dynamic worker thread pool, if enabled, changes the number of worker threads on the fly. The requests are all logged in an outputted file.

Extra credit implemented: Part A AND Part B

Policy for changing the dynamic worker pool thread size: The number of worker threads should correspond one-to-one with the number of items in the queue that haven't already been retrieved by worker threads.

Caching mechanism: LFU (least frequently used cache block is replaced)

Team member contributions:
Shane: Main method, logging, initial dispatch and worker thread mechanisms
Kieran: Synchronization, dynamic worker pool, documentation
Clara: Caching, error handling

