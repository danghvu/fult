#ifndef FULT_CONFIG_H_
#define FULT_CONFIG_H_

/* Stack size, main stack has to be large enough. */
#define F_STACK_SIZE (4 * 1024)
#define MAIN_STACK_SIZE (16 * 1024)

/* Set affinity or not */
#define USE_AFFI
// #define AFF_DEBUG

/* Only 1 or 2 */
#define NLAYER 2

/* Steal policy. */
#define USE_STEAL
#define USE_STEAL_BIT
#define USE_STEAL_ONCE
#define STEAL_POLL 8

/* Wait instead of yield. */
#define USE_COMPLEX_JOIN

/* Run parent directly if possible. */
#define USE_YIELD_BACK 

#endif
