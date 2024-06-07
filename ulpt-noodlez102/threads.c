#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <assert.h>
#include "ec440threads.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX_THREADS 128			/* number of threads you support */
#define THREAD_STACK_SIZE (1<<15)	/* size of stack in bytes */
#define QUANTUM (50 * 1000)		/* quantum in usec */

/* 
   Thread_status identifies the current state of a thread. What states could a thread be in?
   Values below are just examples you can use or not use. 
 */
enum thread_status
{
 TS_EXITED,
 TS_RUNNING,
 TS_READY,
 TS_EMPTY,
 TS_BLOCKED
};

/* The thread control block stores information about a thread. You will
 * need one of this per thread. What information do you need in it? 
 * Hint, remember what information Linux maintains for each task?
 */
struct thread_control_block {
  pthread_t tid;
	void *stack;
	jmp_buf regs;
	enum thread_status status;
  void * exit_status;
};

struct thread_control_block threads[MAX_THREADS];	
pthread_t current_tid = 0;									
struct sigaction signal_handler;	
int exited_string =0;
int num_threads =0;

static void schedule(int signal)
{
  /* 
     TODO: implement your round-robin scheduler, e.g., 
     - if whatever called us is not exiting 
       - mark preempted thread as runnable
       - save state of preempted thread
     - determin which thread should be running next
     - mark thread you are context switching to as running
     - restore registers of that thread
   */
  switch (threads[current_tid].status) {
    case TS_RUNNING:
      threads[current_tid].status = TS_READY;
      break;
    case TS_EXITED:
    case TS_READY:
    case TS_BLOCKED:
    case TS_EMPTY:
      break;
  }
  pthread_t next_tid = current_tid;
	while(1){
		if(next_tid == MAX_THREADS - 1){
			next_tid = 0;
		}
		else{
			next_tid++;
		}

		if(threads[next_tid].status == TS_READY){
			break;
		}
	}

	int jump = 0;
	if(threads[current_tid].status != TS_EXITED){
		jump = setjmp(threads[current_tid].regs);
	}

	if(!jump){
		current_tid = next_tid;
		threads[current_tid].status = TS_RUNNING;
		longjmp(threads[current_tid].regs, 1);
	}
}

// to supress compiler error saying these static functions may not be used...
static void schedule(int signal) __attribute__((unused));


static void scheduler_init()
{
  /* 
     TODO: do everything that is needed to initialize your scheduler.
     For example:
     - allocate/initialize global threading data structures
     - create a TCB for the main thread. so your scheduler will be able to schedule it
     - set up your timers to call scheduler...
  */
 	for(int i = 0; i < MAX_THREADS; i++){
		threads[i].status = TS_EMPTY;
		threads[i].tid = i;
    threads[i].stack = NULL;
	}
  num_threads=1;
  threads[0].status=TS_READY;
  ualarm(QUANTUM, QUANTUM);
  struct sigaction sa;
  sa.sa_handler = schedule; 
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NODEFER; 
  sigaction(SIGALRM, &sa, NULL);
}

int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg)
{
  // Create the timer and handler for the scheduler. Create thread 0.
  static bool is_first_call = true;
  int main_thread =0;
  if (is_first_call) {
    is_first_call = false;
    scheduler_init();
    threads[0].status = TS_READY;
		main_thread = setjmp(threads[0].regs);
  }
  
  /* TODO: Return 0 on successful thread creation, non-zero for an error.
   *       Be sure to set *thread on success.
   *
   * You need to create and initialize a TCB (thread control block) including:
   * - Allocate a stack for the thread
   * - Set up the registers for the functions, including:
   *   - Assign the stack pointer in the thread's registers to point to its stack. 
   *   - Assign the program counter in the thread's registers.
   *   - figure out how to have pthread_exit invoked if thread returns
   * - After you are done, mark your new thread as READY
   * Hint: Easiest to use setjmp to save a set of registers that you then modify, 
   *       and look at notes on reading/writing registers saved by setjmp using 
   * Hint: Be careful where the stackpointer is set to it may help to draw
   *       an empty stack diagram to answer that question.
   * Hint: Read over the comment in header file on start_thunk before 
   *       setting the PC.
   * Hint: Don't forget that the thread is expected to "return" to pthread_exit after it is done
   * 
   * Don't forget to assign RSP too! Functions know where to
   * return after they finish based on the calling convention (AMD64 in
   * our case). The address to return to after finishing start_routine
   * should be the first thing you push on your stack.
   */


  if(!main_thread){
   int tid =1;
  for(tid = 1; tid<MAX_THREADS; tid++){
    if((threads[tid].stack == NULL)){
      break;
    }
  }
  if(num_threads>=MAX_THREADS){
    return -1;
  }
    unsigned long int pc = (unsigned long int)start_thunk;
    set_reg(&threads[tid].regs, JBL_PC, pc);

    set_reg(&threads[tid].regs, JBL_R13, (unsigned long int)arg);

    set_reg(&threads[tid].regs, JBL_R12, (unsigned long int)start_routine);
            
    threads[tid].stack = (void*)malloc(THREAD_STACK_SIZE);  

    unsigned long int stackPointer = (unsigned long int)threads[tid].stack + THREAD_STACK_SIZE - 8;
    *((unsigned long int *)stackPointer) = (unsigned long int)pthread_exit;
    set_reg(&threads[tid].regs, JBL_RSP, (unsigned long int)stackPointer);
            
    threads[tid].status = TS_READY;

    threads[tid].tid = num_threads;
    *thread = num_threads;
    num_threads++;
  }else{
    main_thread =0;
  }
  
  return 0;
}

void pthread_exit(void *value_ptr)
{
  /* TODO: Exit the current thread instead of exiting the entire process.
   * Hints:
   * - Release all resources for the current thread. CAREFUL though.
   *   If you free() the currently-in-use stack then do something like
   *   call a function or add/remove variables from the stack, bad things
   *   can happen.
   * - Update the thread's status to indicate that it has exited
   * What would you do after this?
   */
  threads[current_tid].status = TS_EXITED;  
  threads[current_tid].exit_status = value_ptr;
  exited_string++;
  ualarm(0,0);
  schedule(0);
  exit(0);
}

pthread_t pthread_self(void)
{
  /* 
   * TODO: Return the current thread instead of -1, note it is up to you what ptread_t refers to
   */
  return threads[current_tid].tid;
}

int pthread_join(pthread_t thread, void **retval)
{
  while(1){     
    if(threads[thread].status == TS_EXITED){
      free(threads[thread].stack);
      *retval = threads[thread].exit_status;
      return 0;
    }else{
      schedule(SIGALRM);
    }
  }
}

/* 
 * Don't implement main in this file!
 * This is a library of functions, not an executable program. If you
 * want to run the functions in this file, create separate test programs
 * that have their own main functions.
 */
/******************************Project 5*********************************************************/
typedef struct Node {
  int tid;
  struct Node *next;
}Node;

typedef struct my_mutex_t {
  int locked;
  struct Node *blocked_threads_head;
  struct Node *blocked_threads_tail;
}my_mutex_t;

typedef struct my_barrier_t {
    int count;
    int threads_blocked;
    struct Node *Blocked_one;
    struct Node *Blocked_two;
    int init;
    bool serialed;
}my_barrier_t;
/*************************************************************************************/
static void lock(){
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);  
  sigprocmask(SIG_BLOCK, &mask, NULL);
}
static void unlock(){
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);  
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

int pthread_mutex_init(pthread_mutex_t *restrict mutex,const pthread_mutexattr_t *restrict attr){
  my_mutex_t *m = (my_mutex_t *)malloc(sizeof(struct my_mutex_t*));
  m->locked = 0; // 0 means unlocked, 1 means locked
  m->blocked_threads_head=NULL;
  m->blocked_threads_tail=NULL;
  mutex->__align = (long )m;
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex){//if error try changing to see it know if mutex is destroyed
  my_mutex_t *m = (my_mutex_t *)mutex->__align;
  if (m->locked !=0) {//if it is locked
    return -1;
  }
  struct Node *node_current;
	struct Node *node_next;

	node_current = m->blocked_threads_head;
	node_next = NULL;
	while (node_current != NULL) //free the linked list of waiting threads
	{
		node_next = node_current->next;
		free(node_current);
		node_current = node_next;
	}
  //free(m);
	free((void *)(mutex->__align));
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex){
  my_mutex_t *m = (my_mutex_t *)mutex->__align;
  lock();
  if(m->locked ==0){
    //printf("There is nothing holding the mutex right now so we give this thread the mutex\n");
    m->locked=1;
    threads[current_tid].status = TS_READY;
    unlock();
    return 0;
  }else{
    //printf("There already is thread holding the mutex\n");
    threads[current_tid].status = TS_BLOCKED;
    struct Node *n = (struct Node*)malloc(sizeof(struct Node));
    n->tid=current_tid;
    if(m->blocked_threads_head==NULL){//if the list is empty, then insert it into head.
      //printf("This is the first thread in the head\n");
      m->blocked_threads_head= n;
      n->next=NULL;

    }else if(m->blocked_threads_tail==NULL){//if there is no tail yet then connect head to tail and insert.
      //printf("this is the 2nd thread into the block list ie tail\n");
      m->blocked_threads_head->next=n;
      m->blocked_threads_tail =n;
      n->next=NULL;

    }else{// now finally if both head and tail is occupied, then replace tail and have old tail point to tail.
      //printf("this is any number after both head and tail has been occupied\n");
      struct Node *old_tail = (struct Node*)malloc(sizeof(struct Node));
      old_tail = m->blocked_threads_tail;
      m->blocked_threads_tail=n;
      n->next=NULL;
      old_tail->next=n;
    }
    schedule(0);
    return -1;
  }
}

int pthread_mutex_unlock(pthread_mutex_t *mutex){
  my_mutex_t *m = (my_mutex_t *)mutex->__align;
  lock();
	if(m->blocked_threads_head==NULL){	// if completely empty, then set mutex to unlocked.
		m->locked = 0;
    unlock();
    return 0;
	}else{// what i do in here is messing up releasing.
    struct Node *next=m->blocked_threads_head->next;
    threads[m->blocked_threads_head->tid].status=TS_READY;
    free(m->blocked_threads_head);
    m->blocked_threads_head=next;
    if(next==NULL){
      m->blocked_threads_tail=NULL;
    }
    if (m->blocked_threads_head == NULL) {
      m->locked = 0;
    }
    unlock();
    schedule(0);
    return 0;
  }
}

int pthread_barrier_init(pthread_barrier_t *restrict barrier,const pthread_barrierattr_t *restrict attr,unsigned count){
  my_barrier_t *b = (my_barrier_t *)malloc(sizeof(struct my_barrier_t));
  b->count=count;
  b->threads_blocked=0;
  b->Blocked_one=NULL;
  b->init=1;
  b->Blocked_two=NULL;
  b->serialed=false;
  barrier->__align = (long) b;		// Align the struct pointer with the barrier
  return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier){
  my_barrier_t *b = (my_barrier_t *)barrier->__align;
  if(b->init!=1||b->threads_blocked!=0){
    return -1;
  }
  b->init=0;
  //free(b);
	free((void *)(barrier->__align));
  return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier){
  my_barrier_t *b = (my_barrier_t *)barrier->__align;

  b->threads_blocked++;

  if (b->init != 1) {  // Calling thread gets blocked
    lock();
    threads[current_tid].status = TS_BLOCKED;

    if (b->Blocked_one == NULL) {
      b->Blocked_one = (struct Node *)malloc(sizeof(struct Node));
      b->Blocked_one->tid = current_tid;
      b->Blocked_one->next = NULL;
    } else {
      struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));
      new_node->tid = current_tid;
      new_node->next = NULL;
      b->Blocked_two->next = new_node;
      b->Blocked_two = new_node;
    }

    b->init = 0;
    unlock();
    schedule(0);
  }

  while (b->threads_blocked < b->count) {  
    schedule(0);
  }

  lock();
  if (b->init == 0) {  // Unblock the calling thread
    b->init = 1;
    threads[b->Blocked_one->tid].status = TS_READY;
    struct Node *temp = b->Blocked_one;
    b->Blocked_one = b->Blocked_one->next;
    free(temp);
  }
  unlock();
  schedule(0);

  if (!b->serialed) {  // The first thread returns PTHREAD_BARRIER_SERIAL_THREAD
    b->threads_blocked=0;
    b->Blocked_one=NULL;
    b->init=1;
    b->Blocked_two=NULL;
    b->serialed=true;
    return PTHREAD_BARRIER_SERIAL_THREAD;
  } else {  
    b->threads_blocked=0;
    b->Blocked_one=NULL;
    b->init=1;
    b->Blocked_two=NULL;
    return 0;
  }
}
