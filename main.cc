#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <deque>
#include <exception>
#include "helper.h"

typedef pair<int, int> queue_element;

void *producer (void *parameter);
void *consumer (void *parameter);
int initSemaphores(int queue_size);
int getJobId(deque <queue_element> buffer);
int generateRandomNumber(int min, int max);

struct ProducerParameters {
  int sem_id;
  int jobs_per_producer;
  deque <pair<int, int>>* buffer;
  int* id;
};

struct ConsumerParameters {
  int sem_id;
  deque <queue_element>* buffer;
  int* id;
};

int main (int argc, char **argv) {

  // check for command line variables
  if (argc != 5){

    cerr << "ERROR! Please provide 4 command line variables:\n\n" <<
      "1. Size of the queue\n" <<
      "2. Number of jobs each producer generates\n" <<
      "3. Number of producers\n" <<
      "4. Number of consumers\n" << endl;

    return (-1);
  }
  
  // assign command line variables
  int queue_size = check_arg(argv[1]);
  int jobs_per_producer = check_arg(argv[2]);
  int number_of_producers = check_arg(argv[3]);
  int number_of_consumers = check_arg(argv[4]);

  if (queue_size == -1 ||
      jobs_per_producer == -1 ||
      number_of_producers == -1 ||
      number_of_consumers == -1){

    cerr << "Command line parameters must be positive integers" << endl;
    return (-1);
  }

  // Circular queue implemented as a double-ended queue structure
  deque <queue_element> buffer;

   // Initialise semaphores
  int sem_id = initSemaphores(queue_size);
  if (sem_id == -1){
    cerr << "ERROR! Could not create semaphores.\n" <<
      "Use linux commands 'ipcs' and 'ipcrm' to fix the issue.\n" <<
      "Reboot as a last resort." << endl;
    return sem_id;
  }

  vector <pthread_t> pthread_ids;
  int producer_id = 1;
  int consumer_id = 1;

  try {

    // Create producers
    ProducerParameters prod_params = {sem_id, jobs_per_producer, &buffer, &producer_id};
    
    for (int i = 0; i < number_of_producers; i++){
      pthread_t id;
      pthread_create (&id, NULL, producer, (void *)&prod_params);
      pthread_ids.push_back(id);
    }
    
    // Create consumers
    ConsumerParameters cons_params = {sem_id, &buffer, &consumer_id};
    
    for (int i = 0; i < number_of_consumers; i++){
      pthread_t id;
      pthread_create (&id, NULL, consumer, (void *)&cons_params);
      pthread_ids.push_back(id);
    }
    
    // Wait for threads to complete
    for (auto id: pthread_ids) {
      pthread_join (id, NULL);
    }
  
    // close semaphores
    sem_close(sem_id);

  } catch (const exception &e) {

    cerr << "Unexpected runtime error: " << e.what();
    return (-1);
  }
  
  return 0;
}


void *producer (void *parameter) {
  
  // Initialise variables
  ProducerParameters params = *(ProducerParameters *) parameter;
  int sem_id = params.sem_id;
  int jobs_per_producer = params.jobs_per_producer;
  deque <queue_element> &buffer = *(params.buffer);
  int &id = *(params.id);
  
  int job_duration;
  int job_id;
  int thread_id;

  // Get thread id
  sem_wait (sem_id, IDS);
  thread_id = id++;
  sem_signal (sem_id, IDS);

  string producer_string = "Producer(" + to_string(thread_id) + "): ";

  string producer_no_more_jobs =
    producer_string + "No more jobs to produce.\n";

  string producer_timeout =
    producer_string + "Timed out! No spaces in the queue.\n";

  // Thread code will repeatedly execute until 'pthread_exit' tells it to stop
  while(1){

    // Create job
    if (jobs_per_producer > 0){
      
      job_duration = generateRandomNumber(1, 10);
      jobs_per_producer--;
      
    } else if (jobs_per_producer == 0) {

      cout << producer_no_more_jobs;
      pthread_exit(0);
    }

    // Waits for a space in the queue before timing out if none available
    int ret = sem_wait_timeout (sem_id, SPACE);
    if (ret == -1) {
      cout << producer_timeout;
      pthread_exit(0);
    }
    
    // CRITICAL REGION START - accessing job queue
    sem_wait (sem_id, MUTEX);
    job_id = getJobId(buffer);
    buffer.push_back({job_id, job_duration});

    cout << producer_string
         << "Job id " << job_id
         << " duration " << job_duration << endl;

    sem_signal (sem_id, MUTEX);
    // CRITICAL REGION END

    sem_signal (sem_id, EMPTY);

    // Each producer sleeps between 1-5s before re-entering the loop
    sleep (generateRandomNumber(1, 5));
  }
}


void *consumer (void *parameter) {
  
  // Declare variables
  ConsumerParameters params = *(ConsumerParameters *) parameter;
  int sem_id = params.sem_id;
  deque <queue_element> &buffer = *(params.buffer);
  int &id = *(params.id);

  int thread_id;

  // Get thread id
  sem_wait (sem_id, IDS);
  thread_id = id++;
  sem_signal (sem_id, IDS);

  string consumer_string =  "Consumer(" + to_string(thread_id) + "): ";

  string consumer_no_jobs_left =
    consumer_string + "No more jobs left.\n";

  // Thread code will repeatedly execute until 'pthread_exit' tells it to stop
  while(1){

    int ret = sem_wait_timeout (sem_id, EMPTY);

    if (ret == -1) {

      cout << consumer_no_jobs_left;

      pthread_exit(0);
    }

    // CRITICAL REGION START - accessing job queue
    sem_wait (sem_id, MUTEX);
    queue_element job = buffer.front();
    buffer.pop_front();

    cout << consumer_string
         << "Job id " << job.first 
         << " executing sleep duration " << job.second << endl;

    sem_signal (sem_id, MUTEX);
    // CRITICAL REGION END

    sem_signal (sem_id, SPACE);

    sleep (job.second);

    string confirmed_string = consumer_string +
      "Job id " +
      to_string(job.first) +
      " completed\n";
    
    cout << confirmed_string;
  }
}


int initSemaphores(int queue_size){

  int err;
  int sem_id;
  
  sem_id = sem_create(SEM_KEY, 4);

  // Semaphore enforcing mutex for the job queue
  err = sem_init (sem_id, MUTEX, 1);
  if (err) return err;

  // Semaphore signalling empty queue
  err = sem_init (sem_id, EMPTY, 0);
  if (err) return err;

  // Semaphore signalling empty space in the queue
  err = sem_init (sem_id, SPACE, queue_size);
  if (err) return err;
  
  // Semaphore enforcing mutex for the ID variable
  err = sem_init (sem_id, IDS, 1);
  if (err) return err;

  return sem_id;
}

// Loops through double ended queue to get the next ID
int getJobId(deque <queue_element> buffer){

  int id = 1;

  for (auto job: buffer) {

    if (id == job.first) {
      id++;
      continue;
    }
  }
  
  return id;
}

// Generate a truly random number within a range
int generateRandomNumber(int min, int max){

  random_device rd; // Obtain a random number from hardware
  mt19937 eng(rd()); // Seed the generator
  uniform_int_distribution<> distr(min, max); // Define the range

  return ((int)distr(eng));
}
