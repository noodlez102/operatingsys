#include "../tls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define TLS_SIZE (PAGE_SIZE * 256)
#define CLONES (128 - 1)
#define NUM_THREADS 10

pthread_t thread_create[NUM_THREADS];
pthread_t thread_clone[NUM_THREADS][CLONES];
char write_buffer[NUM_THREADS][PAGE_SIZE], read_buffer[NUM_THREADS][PAGE_SIZE];
const char *str = "this is a random string to test tls";

static void *clone(void *arg) {
    long i = (long)arg;
    printf("Thread: %lu cloning TLS from Thread: %lu\n", (size_t)pthread_self(), (size_t)thread_create[i]);
    assert(tls_clone(thread_create[i]) == 0);

    tls_read(PAGE_SIZE, PAGE_SIZE, read_buffer[i]);
    assert(memcmp(read_buffer[i], write_buffer[i], PAGE_SIZE) == 0);

    if (++i < CLONES) {
        pthread_create(&thread_clone[i], NULL, &clone, (void *)i);
        pthread_join(thread_clone[i], NULL);
    }
    assert(tls_destroy() == 0);
    return NULL;
}

static void *create(void *arg) {
    long thread_num = (long)arg;
    assert(tls_create(TLS_SIZE) == 0);
    printf("Thread: %lu created TLS\n", (size_t)thread_create[thread_num]);

    strcpy(write_buffer[thread_num], str);
    tls_write(PAGE_SIZE, PAGE_SIZE, write_buffer[thread_num]);

    pthread_create(&thread_clone[thread_num][0], NULL, &clone, (void *)thread_num);
    pthread_join(thread_clone[thread_num][0], NULL);

    assert(tls_destroy() == 0);
    return NULL;
}

int main() {
    printf("Main thread: %lu\n", (size_t)pthread_self());
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&thread_create[i], NULL, &create, (void *)i);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(thread_create[i], NULL);
    }
    return 0;
}
