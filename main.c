#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <stdbool.h>

typedef uint64_t u64;
typedef atomic_uint_fast64_t au64;

u64 nr_cpu = 32;
bool fix = false;
bool pin = false;

au64 elapsed = 0;

void thread_pin(const u64 cpu)
{
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);
}

struct worker_info {
    u64 id;
    pthread_barrier_t *barrier;
};

void parse(const int argc, const char *const argv[])
{
    if (argc < 4) {
        printf("parameters: nr_cpu(int) pinning(bool) fix(bool)\n");
        exit(1);
    }

    nr_cpu = (u64)strtoll(argv[1], NULL, 10);
    if (strcmp(argv[2], "true") == 0) {
        pin = true;
    }
    if (strcmp(argv[3], "true") == 0) {
        fix = true;
    }

    printf("nr_cpu: %lu pin: %s fix: %s\n",
            nr_cpu,
            pin ? "yes": "no",
            fix ? "yes": "no");
}

void alloc() {
    for (u64 i=0; i<10000; i++) {
        void *ptrs[16];
        for (u64 j=0; j<16; j++) {
            ptrs[j] = malloc(8);
        }
        for (u64 j=0; j<16; j++) {
            free(ptrs[j]);
        }
    }
}

void *worker(void *const ptr)
{
    const struct worker_info *const wi = (typeof(wi))ptr;
    const u64 id = wi->id;

    if (fix) {
        free(malloc(8));
    }

    if (pin) {
        thread_pin(id);
    }

    pthread_barrier_t *const barrier = wi->barrier;
    pthread_barrier_wait(barrier);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    alloc();

    clock_gettime(CLOCK_MONOTONIC, &end);
    const uint64_t time = ((u64)end.tv_sec - (u64)start.tv_sec) * 1000000000lu
                        + ((u64)end.tv_nsec - (u64)start.tv_nsec);
    elapsed += time;
    return NULL;
}

int main(const int argc, const char *const argv[])
{
    parse(argc, argv);

    pthread_t threads[256];
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, nr_cpu);

    struct worker_info wi[nr_cpu];
    for (u64 i=0; i<nr_cpu; i++) {
        wi[i].id = i;
        wi[i].barrier = &barrier;
        pthread_create(&threads[i], NULL, worker, (void *)&wi[i]);
    }

    for (u64 i=0; i<nr_cpu; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("thread average (ms): %lf\n", (double)elapsed / 1e6 / nr_cpu);

    return 0;
}
