#include "tls.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#define MAX_THREAD_COUNT 128


/*
 * This is a good place to define any data structures you will use in this file.
 * For example:
 *  - struct TLS: may indicate information about a thread's local storage
 *    (which thread, how much storage, where is the storage in memory)
 *  - struct page: May indicate a shareable unit of memory (we specified in
 *    homework prompt that you don't need to offer fine-grain cloning and CoW,
 *    and that page granularity is sufficient). Relevant information for sharing
 *    could be: where is the shared page's data, and how many threads are sharing it
 *  - Some kind of data structure to help find a TLS, searching by thread ID.
 *    E.g., a list of thread IDs and their related TLS structs, or a hash table.
 */
typedef struct thread_local_storage
{
	pthread_t tid;
	unsigned int size; /* size in bytes */
	unsigned int page_num; /* number of pages */
	struct page **pages; /* array of pointers to pages */
} TLS;

struct page {
	unsigned long int address; /* start address of page */
	int ref_count; /* counter for shared pages */
};
struct tid_tls_pair
{
	pthread_t tid;
	TLS *tls;
};
/*
 * Now that data structures are defined, here's a good place to declare any
 * global variables.
 */

static struct tid_tls_pair
tid_tls_pairs[MAX_THREAD_COUNT];
int page_size;
bool initialize=false;
/*
 * With global data declared, this is a good point to start defining your
 * static helper functions.
 */

void tls_handle_page_fault(int sig, siginfo_t *si, void *context)
{
	unsigned long int p_fault = ((unsigned long int) si->si_addr) & ~(page_size - 1);
	for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        TLS *tls = tid_tls_pairs[i].tls;
        for (int j = 0; j < tls->page_num; j++) {
            if (tls->pages[j]->address == p_fault) {
				printf("Seg Fault Thread");
                pthread_exit(NULL);
            }
        }
    }
	
	printf("Seg Fault");
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	raise(sig);
}

void tls_init()
{
	struct sigaction sigact;
	page_size = getpagesize();
	/* Handle page faults (SIGSEGV, SIGBUS) */
	sigemptyset(&sigact.sa_mask);
	/* Give context to handler */
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = tls_handle_page_fault;
	sigaction(SIGBUS, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL);
	initialize=true;
	for (int i = 0; i < MAX_THREAD_COUNT; i++)
	{
		tid_tls_pairs[i].tid = -1; //For now, set all thread IDs to -1
	}
}

void tls_protect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, PROT_NONE)) {
	fprintf(stderr, "tls_protect: could not protect page\n");
	exit(1);
	}
}

void tls_unprotect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, PROT_READ | PROT_WRITE)) {
	fprintf(stderr, "tls_unprotect: could not unprotect page\n");
	exit(1);
	}
}

/*
 * Lastly, here is a good place to add your externally-callable functions.
 */ 

int tls_create(unsigned int size)
{
	if (initialize == false)
	{
		tls_init();
	}

	if (size <= 0) 
	{
		return -1;
	}

	int new_tls_index = -1;
	for (int i = 0; i < MAX_THREAD_COUNT; i++) //need to find next open spot to store TID and TLS pair
	{
		if (tid_tls_pairs[i].tid == -1) //then the spot is available
		{
			new_tls_index = i;
			break;
		}
	}

    pthread_t self_tid = pthread_self();

    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == self_tid) {
            return -1; // Error: Thread already has a TLS
        }
    }
	struct thread_local_storage* new_tls = calloc(1, sizeof(struct thread_local_storage)); 
	new_tls->tid= self_tid;
	new_tls->size = size;
    new_tls->page_num = (size - 1) / page_size + 1;
	new_tls->pages = (struct page **)calloc(new_tls->page_num, sizeof(struct page *));
	for(int i =0; i < new_tls->page_num; i++){
		struct page *p = (struct page *)malloc(sizeof(struct page));
		p->address = (unsigned long int)mmap(0, page_size, PROT_NONE,MAP_ANON | MAP_PRIVATE,0, 0);
		p->ref_count = 1;
		new_tls->pages[i] = p;
	}
	tid_tls_pairs[new_tls_index].tls = new_tls;
	tid_tls_pairs[new_tls_index].tid = pthread_self();
	return 0;
}

int tls_destroy()
{
	pthread_t self_tid = pthread_self();
    TLS *tls = NULL;

    // Find TLS associated with the current thread
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == self_tid) {
            tls = tid_tls_pairs[i].tls;
            break;
        }
    }

    // Error handling: check if current thread has TLS
    if (tls == NULL) {
        return -1; 
    }

    for (unsigned int i = 0; i < tls->page_num; i++) {
        if (tls->pages[i]->ref_count == 1) {
            munmap((void *)tls->pages[i]->address, page_size);
            free(tls->pages[i]);
        }
        else {
            tls->pages[i]->ref_count--;
        }
    }

    free(tls->pages);
    free(tls);

    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == self_tid) {
            tid_tls_pairs[i].tid = 0;
            tid_tls_pairs[i].tls = NULL;
            break;
        }
    }

	return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
	pthread_t self_tid = pthread_self();
    TLS *tls = NULL;

    // Find TLS associated with the current thread
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == self_tid) {
            tls = tid_tls_pairs[i].tls;
            break;
        }
    }
    if (tls == NULL) {
        return -1; // Error: No TLS found for current thread
    }

    if (offset + length > tls->size) {
        return -1; // Error: Offset + length exceeds TLS size
    }

    for (unsigned int i = 0; i < tls->page_num; i++) {
        tls_unprotect(tls->pages[i]);
    }

	int cnt,idx;
	for ( cnt = 0,idx = offset; idx < (offset +length); ++cnt, ++idx) {
		struct page *p;
		unsigned int pn, poff;
		pn = idx / page_size;
		poff = idx % page_size;
		p = tls->pages[pn];
		char *src = ((char *) p->address) + poff;
		buffer[cnt] = *src;
	}
	for (unsigned int i = 0; i < tls->page_num; i++) {
        tls_protect(tls->pages[i]);
    }
	return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer)
{
	pthread_t self_tid = pthread_self();
    TLS *tls = NULL;

    // Find TLS associated with the current thread
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == self_tid) {
            tls = tid_tls_pairs[i].tls;
            break;
        }
    }
    if (tls == NULL) {
        return -1; // Error: No TLS found for current thread
    }

    if (offset + length > tls->size) {
        return -1; // Error: Offset + length exceeds TLS size
    }

    for (unsigned int i = 0; i < tls->page_num; i++) {
        tls_unprotect(tls->pages[i]);
    }
	int cnt,idx;
	for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
		struct page *p, *copy;
		unsigned int pn, poff;
		pn = idx / page_size;
		poff = idx % page_size;
		p = tls->pages[pn];
		if (p->ref_count > 1) {
			copy = (struct page *) calloc(1, sizeof(struct page));
			copy->address = (unsigned long int) mmap(0,page_size, PROT_WRITE,MAP_ANON | MAP_PRIVATE, 0, 0);
			copy->ref_count = 1;
			memcpy((void *) copy->address, (void *) p->address, page_size);
			tls->pages[pn] = copy;
			/* update original page */
			p->ref_count--;
			tls_protect(p);
			p = copy;
		}
		char * dst = ((char *) p->address) + poff;
		*dst = buffer[cnt];
	}
	for (unsigned int i = 0; i < tls->page_num; i++) {
        tls_protect(tls->pages[i]);
    }
	return 0;
}

int tls_clone(pthread_t tid)
{
	pthread_t self_tid = pthread_self();
    TLS *self_tls = NULL;
    TLS *target_tls = NULL;

    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == self_tid) {
            self_tls = tid_tls_pairs[i].tls;
            if (self_tls != NULL) {
                return -1; // Error: Current thread already has a TLS
            }
            break;
        }
    }

    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == tid) {
            target_tls = tid_tls_pairs[i].tls;
            if (target_tls == NULL) {
                return -1; // Error: Target thread does not have a TLS
            }
            break;
        }
    }
	
	if (target_tls == NULL) {
		printf("Error: Target thread %lu not found in tid_tls_pairs\n", (unsigned long)tid);
		return -1;
	}

    TLS *new_tls = calloc(1, sizeof(TLS));
    new_tls->tid = self_tid;
    new_tls->size = target_tls->size;
    new_tls->page_num = target_tls->page_num;
    new_tls->pages = (struct page **)calloc(new_tls->page_num, sizeof(struct page *));

    for (unsigned int i = 0; i < new_tls->page_num; i++) {
        struct page *target_page = target_tls->pages[i];
        new_tls->pages[i] = target_page;
        target_page->ref_count++;
    }

    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (tid_tls_pairs[i].tid == -1) {
            tid_tls_pairs[i].tid = self_tid;
            tid_tls_pairs[i].tls = new_tls;
            break;
        }
    }

	return 0;
}
