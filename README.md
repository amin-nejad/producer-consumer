# producer-consumer
Operating Systems Coursework

This program implements the producer-consumer scenario using POSIX threads, where each producer
and consumer will be running in a different thread. The shared data structure used will be a circular
queue. Semaphores are used to restrict access to this circular queue.

The program reads in four command line arguments: 

- size of the queue
- number of jobs to generate for each producer (each producer will generate the same number of jobs)
- number of producers
- number of consumers

e.g.

```
./main 5 3 2 2
```
