#ifndef DISPATCHER_H
#define DISPATCHER_H
 
//The barber, returns 0 on success.
int dispatcher_start(void);
 
//Stops Queue, lets all jobs finish and then join.
void dispatcher_stop(void);
 
#endif //DISPATCHER_H
 
