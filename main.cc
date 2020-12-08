/******************************************************************
 * The Main program with the two functions. A simple
 * example of creating and using a thread is provided.
 ******************************************************************/

#include <deque>
#include <vector>
#include <semaphore.h>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <time.h>

#include "helper.h"

void *producer(void *id);
void *consumer(void *id);

// Initialize 3 semaphores - one for mutual exclusion, one for checking if buffer is empty, one for checking if buffer is full
sem_t empty_count;        // Check if buffer is not empty
sem_t full_count;         // Check if buffer is not full
sem_t queue_access_mutex; // mutual exclusion semaphore

// 4 command line arguments to be parsed from argv
int queue_size;         
int number_of_jobs_for_each_producer;
int number_of_producers;
int number_of_consumers;

// 2 timer variables - one for consumer + one for producer - to check the 20-second timeouts
struct timespec ts_consumer;
struct timespec ts_producer;

// job class - simply contains the 'duration' - generated by the producer and consumed by sleeping for 'duration' seconds in the consumer
struct job
{
  job() {}
  job(int id_, int t) : id(id_), duration(t) {}

  int id;
  int duration;
};

std::deque<job> Q;  // Entity to hold the circular buffer contents

using namespace std;


int main(int argc, char *argv[])
{

// Arguments have to contain program name + 4 variables relating to sizing of queue, jobs and number of producers and consumers
  if (argc < 5)
  {
    cerr << "Insufficient number of input parameters" << endl;
    return 0;
  }

  // Parse command line arguments into variables
  queue_size = check_arg (argv[1]);
  number_of_jobs_for_each_producer = check_arg (argv[2]);
  number_of_producers =check_arg (argv[3]);
  number_of_consumers = check_arg (argv[4]);

  // Initialize the 3 semaphores
  sem_init(&empty_count, 0, queue_size); // to ensure buffer is not full - initially size of 'empty = queue size'
  sem_init(&full_count, 0, 0);           // 
  sem_init(&queue_access_mutex, 0, 1);   // mutex

  // Create arrays of threads
  pthread_t consumer_threads[number_of_consumers];
  pthread_t producer_threads[number_of_producers];

  // Two temporary arrays to hold thread id's
  int *temp;
  int *temp2;

  // Iterative variables to loop through number of consumers and number of producers and create a separate thread for each
  int i = 0;
  int j = 0;

  // Create an array of integers - the contents will be used to give threads id's
  temp = new (nothrow) int[number_of_consumers];
  temp2 = new (nothrow) int[number_of_producers];

  // Summarize settings entered
  cout << "Entered: consumers = " << number_of_consumers << " | producers = " << number_of_producers << " | queue_size = " << queue_size << " | number_of_jobs_for_each_producer = " << number_of_jobs_for_each_producer << endl;

  // Loop from 0 to number of consumers and create threads
  while (i < number_of_consumers)
  {
    temp[i] = i;
    pthread_create(&consumer_threads[i], NULL, consumer, (void *)&temp[i]);
    ++i;
  }

  cout << "..Created all consumer threads..";

  // Loop from 0 to number of producers and create threads
  while (j < number_of_producers)
  {
    temp2[j] = j;
    pthread_create(&producer_threads[j], NULL, producer, (void *)&temp2[j]);
    ++j;
  }

  cout << "..Created all producer threads..\n\n";

  // Join all threads
  for (int i = 0; i < number_of_producers; i++)
  {
    pthread_join(producer_threads[i], NULL); 
  }

  for (int i = 0; i < number_of_consumers; i++)
  {
    pthread_join(consumer_threads[i], NULL); 
  }

  cout << "\n\nJoined all threads..";
  cout << "..cleanup starts..";

  // Clean up the semaphores
  sem_destroy(&empty_count);
  sem_destroy(&full_count);
  sem_destroy(&queue_access_mutex);

  // Delete temporary variables
  delete[] temp;
  delete[] temp2;

  cout << "..exiting..\n";
  return 0;
}




void *producer(void *id)
{
  int *producer_id = (int *)id; // give the thread an id from loop which created the thread
  int producer_timer_result = 0;  // timer variable based on the 20-second timed wait. If this times out value becomes -1
  
  string msg_str;
  msg_str = "Started producer thread = " + to_string(*producer_id);

  cout << msg_str;

  for (int p = 0; p < number_of_jobs_for_each_producer; p++)  // Loop through 0 to number of jobs for each producer
  {
    int sleep_time = (rand() % 5) + 1;  // Generate random sleep time between 1-5 seconds
    int duration = (rand() % 10) + 1;   // Generate random sleep time for each job - between 1-10 seconds

    cout << "Producer(" << *producer_id << ") about to go to sleep for " << sleep_time << " seconds.." << endl;

    sleep(sleep_time);                  // Sleep

    cout << "Producer(" << *producer_id << ") woke up.." << endl;

    int current_number_of_items_in_buffer = Q.size(); 
    int job_id = p;                      // Set job id to current producer loop iteration

    cout << "Producer(" << *producer_id << "): Job id " << job_id << "..current number of jobs in buffer = " << current_number_of_items_in_buffer << endl;

    // Set the timer starting point
    clock_gettime(CLOCK_REALTIME, &ts_producer);
    ts_producer.tv_sec += 20;

    // Start critical section
    producer_timer_result = sem_timedwait(&empty_count, &ts_producer);  // First semaphore is timed

    if (producer_timer_result == -1)    // sem_timedwait returns -1 if timed out. Exit if -1 is returned
    {
      cout << "Producer(" << *producer_id << "): Job id " << job_id << " timed out" << endl;
      break; // if timer times out - break
    } 

    sem_wait(&queue_access_mutex); // Wait for 2nd semaphore - the mutex

    // Check if product id is already used in current queue - if it is used - then set one which is not used
    for (int i = 0; i < current_number_of_items_in_buffer; ++i)
    {
      auto it = find_if(Q.begin(), Q.end(), [&p, &current_number_of_items_in_buffer, &i](const job &obj) { return obj.id == ((p + i) % current_number_of_items_in_buffer); });
      if (it != Q.end())
      {
        job_id = ((p + i) % current_number_of_items_in_buffer);
        break;
      }
    }

    job J = job(job_id, duration);  // Create new job with duration (randomly generated 1-10 seconds) and id

    Q.push_back(J); // Add job to queue
    cout << "Producer(" << *producer_id << "): Job id " << job_id << " sleeping for " << sleep_time << " and produced job with duration = " << duration << endl;

    sem_post(&queue_access_mutex);
    sem_post(&full_count);
    // Exit critical section

   } // for loop ends

  pthread_exit(0);  // exit thread
}



void *consumer(void *id)
{
  int *consumer_id = (int *)id;   // give the thread an id from loop which created the thread
  job J_copy;                     // temporary job object
  int consumer_timer_result = 0;  // timer variable to check if no timeout has occured during wait. Set to -1 if 20-second timeout reached

  cout << "\nStarting consumer thread = " << *consumer_id << endl; 

  while (true)                    // Consumer loops until broken out by the timeout loop
  {
    clock_gettime(CLOCK_REALTIME, &ts_consumer);
    ts_consumer.tv_sec += 20;

    // Critical section starts
    consumer_timer_result = sem_timedwait(&full_count, &ts_consumer); // timer will return -1 if times out
    
    if (consumer_timer_result == -1)                                  // -1 is the code for timeout
    {
      break;
    }

    sem_wait(&queue_access_mutex); // Now check the mutex

    if (Q.size() > 0)               // If there are jobs - take one
    {
      job J = Q.front();
      J_copy = job(J.id, J.duration);
      Q.pop_front();
      cout << "Consumer(" << *consumer_id << "): Job id " << J_copy.id << " grabbed item..about to consume for t = " << J_copy.duration << "..items left in queue = " << Q.size() << endl;
    }

    sem_post(&queue_access_mutex);
    sem_post(&empty_count);
    // Critical section ends

    sleep(J_copy.duration);         // Consume the job which was grabbed in critical section
 
    cout << "Consumer(" << *consumer_id << "): Job id " << J_copy.id << " consumption done.." << endl;
  } // while true loop ends

  if (consumer_timer_result == -1)  // Code arrives here if timer result is -1 - the code has timed out
  {
    cout << "Consumer(" << *consumer_id << "): Job id " << J_copy.id << " timed out waiting..exiting thread.." << endl;
  }

  pthread_exit(0);  // exit thread
}