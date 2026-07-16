/* random.c — the BSD random family (random/srandom/initstate/setstate),
 * REWRITTEN from musl src/prng/random.c: musl's internal spinlock (lock.h
 * LOCK/UNLOCK + fork-lock registration) is replaced by a static pthread_mutex
 * woven through all four entry points — same mutual exclusion, our lock
 * layer; c2go has no fork, so the registration is dropped. That lock weave is
 * why this TU is csrc while the lock-free rand48 family stays verbatim in the
 * musl fork (src/prng/). */
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <c2go.h>

/* musl names this array `init`; renamed in-source during #675 when a Plan 9
 * DATA symbol named `init` still collided with the Go linker's per-package
 * initializer TEXT symbol `<pkg>.init` (the x relocation resolved into CODE
 * and srandom's first store SIGBUSed). #676 has since root-fixed this in
 * clang — data symbols named init/main are now auto-renamed to
 * c2go_dinit/c2go_dmain (the #317 function treatment) — so this source
 * rename is no longer load-bearing; kept for grep-vs-musl clarity. */
static uint32_t init_tbl[] = {
0x00000000,0x5851f42d,0xc0b18ccf,0xcbb5f646,
0xc7033129,0x30705b04,0x20fd5db4,0x9a8b7f78,
0x502959d8,0xab894868,0x6c0356a7,0x88cdb7ff,
0xb477d43f,0x70a3a52b,0xa8e4baf1,0xfd8341fc,
0x8ae16fd9,0x742d2f7a,0x0d1f0796,0x76035e09,
0x40f7702c,0x6fa72ca5,0xaaa84157,0x58a0df74,
0xc74a0364,0xae533cc4,0x04185faf,0x6de3b115,
0x0cab8628,0xf043bfa4,0x398150e9,0x37521657};

static int n = 31;
static int i = 3;
static int j = 0;
static uint32_t *x = init_tbl+1;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t lcg31(uint32_t x) {
	return (1103515245*x + 12345) & 0x7fffffff;
}

static uint64_t lcg64(uint64_t x) {
	return 6364136223846793005ull*x + 1;
}

static void *savestate() {
	x[-1] = (n<<16)|(i<<8)|j;
	return x-1;
}

static void loadstate(uint32_t *state) {
	x = state+1;
	n = x[-1]>>16;
	i = (x[-1]>>8)&0xff;
	j = x[-1]&0xff;
}

static void __srandom(unsigned seed) {
	int k;
	uint64_t s = seed;

	if (n == 0) {
		x[0] = s;
		return;
	}
	i = n == 31 || n == 7 ? 3 : 1;
	j = 0;
	for (k = 0; k < n; k++) {
		s = lcg64(s);
		x[k] = s>>32;
	}
	/* make sure x contains at least one odd number */
	x[0] |= 1;
}

c2go_extern void srandom(unsigned seed) {
	pthread_mutex_lock(&lock);
	__srandom(seed);
	pthread_mutex_unlock(&lock);
}

c2go_extern char *initstate(unsigned seed, char *state, size_t size) {
	void *old;

	if (size < 8)
		return 0;
	pthread_mutex_lock(&lock);
	old = savestate();
	if (size < 32)
		n = 0;
	else if (size < 64)
		n = 7;
	else if (size < 128)
		n = 15;
	else if (size < 256)
		n = 31;
	else
		n = 63;
	x = (uint32_t*)state + 1;
	__srandom(seed);
	savestate();
	pthread_mutex_unlock(&lock);
	return old;
}

c2go_extern char *setstate(char *state) {
	void *old;

	pthread_mutex_lock(&lock);
	old = savestate();
	loadstate((uint32_t*)state);
	pthread_mutex_unlock(&lock);
	return old;
}

c2go_extern long random(void) {
	long k;

	pthread_mutex_lock(&lock);
	if (n == 0) {
		k = x[0] = lcg31(x[0]);
		goto end;
	}
	x[i] += x[j];
	k = x[i]>>1;
	if (++i == n)
		i = 0;
	if (++j == n)
		j = 0;
end:
	pthread_mutex_unlock(&lock);
	return k;
}
