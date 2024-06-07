#include "ec440threads.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void *hello(void *arg) {
  printf("Hello from thread %ld\n", pthread_self());
  pthread_exit(&arg);
  return NULL;
}

int main() {
  pthread_t thread;
  void *pret;
  long test;

  srand(time(NULL));
  test = rand();

  pthread_create(&thread, NULL, hello, (void *)test);
  schedule(0);
  pthread_join(thread, &pret);
  assert(*(long *)pret == test);
}
