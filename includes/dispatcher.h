#ifndef DISPATCHER_H
#define DISPATCHER_H
 
//The barber, returns 0 on success.
int dispatcher_start(void);
 
//Stops Queue, lets all jobs finish and then join.
void dispatcher_stop(void);

int dispatcher_get_status(unsigned long id, int *step, int *total);
int dispatcher_peek(unsigned long id, int i, int j, int *step, double *val);
#endif //DISPATCHER_H
 
