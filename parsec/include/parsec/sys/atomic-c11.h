/*
 * Copyright (c) 2016-2017 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include <stdatomic.h>
#include <time.h>

ATOMIC_STATIC_INLINE
void parsec_mfence(void)
{
    atomic_thread_fence(memory_order_seq_cst);
}

/**
 * This is extremely ugly but apparently it is the only way to correctly coherce
 * the compilers to convert to the correct type. Thanks to StackOverflow for the
 * tip (http://stackoverflow.com/questions/22851465/typeof-uses-in-c-besides-macros).
 */
#define parsec_atomic_band(LOCATION, AND_VALUE)  \
    (((__typeof__(*(LOCATION)))atomic_fetch_and((_Atomic __typeof__(*(LOCATION))(*))(LOCATION), (AND_VALUE))) & (AND_VALUE))

#define parsec_atomic_bor(LOCATION, OR_VALUE)  \
    (((__typeof__(*(LOCATION)))atomic_fetch_or((_Atomic __typeof__(*(LOCATION))(*))(LOCATION), (OR_VALUE))) | (OR_VALUE))

ATOMIC_STATIC_INLINE
uint32_t parsec_atomic_set_mask(uint32_t* location, uint32_t mask)
{
    return parsec_atomic_bor(location, mask);
}

ATOMIC_STATIC_INLINE
uint32_t parsec_atomic_clear_mask(uint32_t* location, uint32_t mask)
{
    return parsec_atomic_band(location, ~mask);
}

ATOMIC_STATIC_INLINE
int32_t parsec_atomic_cas_32b(volatile uint32_t* location,
                              uint32_t old_value,
                              uint32_t new_value)
{
    return atomic_compare_exchange_strong( (atomic_uint*)location, &old_value, new_value );
}

ATOMIC_STATIC_INLINE
int32_t parsec_atomic_cas_64b(volatile uint64_t* location,
                              uint64_t old_value,
                              uint64_t new_value)
{
    if (sizeof(atomic_ulong) == sizeof(uint64_t))
        return atomic_compare_exchange_strong( (atomic_ulong*)location, &old_value, new_value );
    if (sizeof(atomic_ulong) == sizeof(uint64_t))
        return atomic_compare_exchange_strong( (atomic_ullong*)location, &old_value, new_value );
    assert(0);  /* not good */
}

ATOMIC_STATIC_INLINE
int32_t parsec_atomic_cas_128b(volatile __uint128_t* location,
                               __uint128_t old_value,
                               __uint128_t new_value)
{
    return atomic_compare_exchange_strong( (_Atomic __uint128_t*)location, &old_value, new_value );
}

#if PARSEC_SIZEOF_VOID_P == 4
#define parsec_atomic_cas_ptr(L, O, N) parsec_atomic_cas_32b( (volatile uint32_t*)(L), \
                                                            (uint32_t)(O), (uint32_t)(N) )
#elif PARSEC_SIZEOF_VOID_P == 8
#define parsec_atomic_cas_ptr(L, O, N) parsec_atomic_cas_64b( (volatile uint64_t*)(L), \
                                                            (uint64_t)(O), (uint64_t)(N) )
#else
#define parsec_atomic_cas_ptr(L, O, N) parsec_atomic_cas_128b( (volatile __uint128_t*)(L), \
                                                             (__uint128_t)(O), (__uint128_t)(N) )
#endif

#define parsec_atomic_add_32b(LOCATION, VALUE)                           \
    _Generic((LOCATION),                                                \
             int32_t* : (atomic_fetch_add((_Atomic int32_t*)(LOCATION), (VALUE)) + (VALUE)), \
             uint32_t*: (atomic_fetch_add((_Atomic uint32_t*)(LOCATION), (VALUE)) + (VALUE)), \
             default: (atomic_fetch_add((_Atomic int32_t*)(LOCATION), (VALUE)) + (VALUE)))

#define parsec_atomic_sub_32b(LOCATION, VALUE)                           \
    _Generic((LOCATION),                                                \
             int32_t* : (atomic_fetch_sub((_Atomic int32_t*)(LOCATION), (VALUE)) - (VALUE)), \
             uint32_t*: (atomic_fetch_sub((_Atomic uint32_t*)(LOCATION), (VALUE)) - (VALUE)), \
             default: (atomic_fetch_sub((_Atomic int32_t*)(LOCATION), (VALUE)) - (VALUE)))

#define parsec_atomic_inc_32b(LOCATION)                                  \
    _Generic((LOCATION),                                                \
             int32_t* : (1 + atomic_fetch_add((_Atomic int32_t*)(LOCATION), 1)), \
             uint32_t*: (1 + atomic_fetch_add((_Atomic uint32_t*)(LOCATION), 1)), \
             default: (1 + atomic_fetch_add((_Atomic int32_t*)(LOCATION), 1)))

#define parsec_atomic_dec_32b(LOCATION)                                  \
    _Generic((LOCATION),                                                \
             int32_t* : (atomic_fetch_sub((_Atomic int32_t*)(LOCATION), 1) - 1), \
             uint32_t*: (atomic_fetch_sub((_Atomic uint32_t*)(LOCATION), 1) - 1), \
             default: (atomic_fetch_sub((_Atomic int32_t*)(LOCATION), 1) - 1))

typedef volatile atomic_flag parsec_atomic_lock_t;

ATOMIC_STATIC_INLINE
void parsec_atomic_lock( parsec_atomic_lock_t* atomic_lock )
{
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 };
    while( atomic_flag_test_and_set(atomic_lock) )
        nanosleep( &ts, NULL ); /* less bandwidth consuming */
}

ATOMIC_STATIC_INLINE
void parsec_atomic_unlock( parsec_atomic_lock_t* atomic_lock )
{
    atomic_flag_clear(atomic_lock);
}

ATOMIC_STATIC_INLINE
long parsec_atomic_trylock( parsec_atomic_lock_t* atomic_lock )
{
    return !atomic_flag_test_and_set(atomic_lock);
}

#define PARSEC_ATOMIC_UNLOCKED 0
#define PARSEC_ATOMIC_HAS_ATOMIC_CAS_128B 1

