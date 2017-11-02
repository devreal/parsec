/*
 * Copyright (c) 2009-2017 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include <assert.h>
#include "parsec/parsec_config.h"
#include "parsec/class/parsec_hash_table.h"
#include "parsec/class/list.h"
#include "parsec/utils/mca_param.h"
#include <stdio.h>

#define HELPFIRST

/**
 * @brief Bucket for hash tables. There is no need to have this structure public, it
 *        should only be used in this file.
 */
struct parsec_hash_table_bucket_s {
    parsec_atomic_lock_t      lock;             /**< Buckets are lockable for multithread access 
                                                 *   We also use this lock to atomically update the
                                                 *   list of elements when needed. */
    uint32_t                  cur_len;          /**< Number of elements currently in this bucket */
    parsec_hash_table_item_t *first_item;       /**< Otherwise they are simply chained lists */
};

#define BASEADDROF(item, ht)  (void*)(  ( (char*)(item) ) - ( (ht)->elt_hashitem_offset ) )
#define ITEMADDROF(ptr, ht)   (parsec_hash_table_item_t*)( ((char*)(ptr)) + ( (ht)->elt_hashitem_offset ) )

static int      parsec_hash_table_mca_param_index     = -1;
static uint32_t parsec_hash_table_max_collisions_hint = 16;

void *parsec_hash_table_item_lookup(parsec_hash_table_t *ht, parsec_hash_table_item_t *item)
{
    return BASEADDROF(item, ht);
}

/* To create object of class parsec_hash_table that inherits parsec_object_t class */
OBJ_CLASS_INSTANCE(parsec_hash_table_t, parsec_object_t, NULL, NULL);

int parsec_hash_tables_init(void)
{
    int v = parsec_hash_table_max_collisions_hint;

    parsec_hash_table_mca_param_index =
        parsec_mca_param_reg_int_name("parsec", "parsec_hash_table_max_collisions_hint",
                                      "Sets a hint for the dynamic hash tables implementation: "
                                      "Hash-tables will be resized if a bucket reaches this number of collisions.\n",
                                      false, false, v, &v);
    if( PARSEC_ERROR != parsec_hash_table_mca_param_index ) {
        parsec_hash_table_max_collisions_hint = v;
        return PARSEC_ERROR;
    }
    return PARSEC_SUCCESS;
}

void parsec_hash_table_init(parsec_hash_table_t *ht, int64_t offset, size_t size_of_table, parsec_hash_table_fn_t *hash, void *data)
{
    parsec_atomic_lock_t unlocked = { PARSEC_ATOMIC_UNLOCKED };
    parsec_atomic_rwlock_t unlock = { 0 };
    parsec_hash_table_head_t *head;

    if( parsec_hash_table_mca_param_index != PARSEC_ERROR ) {
        int v;
        if( parsec_mca_param_lookup_int(parsec_hash_table_mca_param_index, &v) != PARSEC_ERROR ) {
            parsec_hash_table_max_collisions_hint = v;
        }
    }        
    
    ht->hash      = hash;
    ht->hash_data = data;
    ht->elt_hashitem_offset = offset;

    head = malloc(sizeof(parsec_hash_table_head_t));
    head->buckets      = malloc(size_of_table * sizeof(parsec_hash_table_bucket_t));
    head->size         = size_of_table;
    head->used_buckets = 0;
    head->next         = NULL;
    head->next_to_free = NULL;
    ht->rw_hash        = head;
    ht->rw_lock        = unlock;

    for( size_t i = 0; i < size_of_table; i++) {
        head->buckets[i].lock = unlocked;
        head->buckets[i].cur_len = 0;
        head->buckets[i].first_item = NULL;
    }
}

void parsec_hash_table_lock_bucket(parsec_hash_table_t *ht, uint64_t key )
{
    uint32_t hash;

    parsec_atomic_rwlock_rdlock(&ht->rw_lock);
    hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);
    assert( hash < ht->rw_hash->size );
    parsec_atomic_lock(&ht->rw_hash->buckets[hash].lock);
}

static void parsec_hash_table_resize(parsec_hash_table_t *ht)
{
    parsec_atomic_lock_t unlocked = { PARSEC_ATOMIC_UNLOCKED };
    parsec_hash_table_head_t *head;
    size_t size_of_table = ht->rw_hash->size * 2;
    
    head = malloc(sizeof(parsec_hash_table_head_t));
    head->buckets      = malloc(size_of_table * sizeof(parsec_hash_table_bucket_t));
    head->size         = size_of_table;
    head->used_buckets = 0;
    head->next         = ht->rw_hash;
    head->next_to_free = ht->rw_hash;
    ht->rw_hash        = head;

    for( size_t i = 0; i < size_of_table; i++) {
        head->buckets[i].lock = unlocked;
        head->buckets[i].cur_len = 0;
        head->buckets[i].first_item = NULL;
    }
}

void parsec_hash_table_unlock_bucket(parsec_hash_table_t *ht, uint64_t key )
{
    int resize;
    parsec_hash_table_head_t *cur_head;
    uint32_t hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);

    assert( hash < ht->rw_hash->size );
    resize = (ht->rw_hash->buckets[hash].cur_len > parsec_hash_table_max_collisions_hint);
    cur_head = ht->rw_hash;
    parsec_atomic_unlock(&ht->rw_hash->buckets[hash].lock);
    parsec_atomic_rwlock_rdunlock(&ht->rw_lock);

    if( resize ) {
        parsec_atomic_rwlock_wrlock(&ht->rw_lock);
        if( cur_head == ht->rw_hash ) {
            /* Barring ABA problems, nobody resized the hash table;
             * Good enough hint that it's our role to do so */
            parsec_hash_table_resize(ht);
        }
        /* Otherwise, let's asssume somebody resized already */
        parsec_atomic_rwlock_wrunlock(&ht->rw_lock);
    }
}

void parsec_hash_table_fini(parsec_hash_table_t *ht)
{
    parsec_hash_table_head_t *head, *next;
    head = ht->rw_hash;
    while( NULL != head ) {
        for(size_t i = 0; i < head->size; i++) {
            assert(NULL == head->buckets[i].first_item);
        }
        next = head->next_to_free;
        free(head->buckets);
        free(head);
        head = next;
    }
}

void parsec_hash_table_nolock_insert(parsec_hash_table_t *ht, parsec_hash_table_item_t *item)
{
    uint32_t hash;
    uint64_t key = item->key;
    int res;
    hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);
    item->next_item = ht->rw_hash->buckets[hash].first_item;
    ht->rw_hash->buckets[hash].first_item = item;
    res = parsec_atomic_inc_32b(&ht->rw_hash->buckets[hash].cur_len);
    if( 1 == res ) {
        parsec_atomic_inc_32b(&ht->rw_hash->used_buckets);
    }
}

static void *parsec_hash_table_nolock_remove_from_old_tables(parsec_hash_table_t *ht, uint64_t key)
{
    parsec_hash_table_head_t *head, *prev_head;
    parsec_hash_table_item_t *current_item, *prev_item;
    uint32_t hash;
    int32_t res;
#if defined(HELPFIRST)
    uint32_t hash_main_bucket = ht->hash(key, ht->rw_hash->size, ht->hash_data);
#endif
    prev_head = ht->rw_hash;
    for(head = ht->rw_hash->next; NULL != head; head = head->next) {
        hash = ht->hash(key, head->size, ht->hash_data);
        prev_item = NULL;
        parsec_atomic_lock(&head->buckets[hash].lock );
        current_item = head->buckets[hash].first_item;
        while( NULL != current_item ) {
            if( current_item->key == key ) {
                if( NULL == prev_item ) {
                    head->buckets[hash].first_item = current_item->next_item;
                } else {
                    prev_item->next_item = current_item->next_item;
                }
                res = parsec_atomic_dec_32b(&head->buckets[hash].cur_len);
                if( 0 == res ) {
                    res = parsec_atomic_dec_32b(&head->used_buckets);
                    if( 0 == res ) {
                        parsec_atomic_cas_ptr(&prev_head->next, head, head->next);
                    }
                }
                parsec_atomic_unlock(&head->buckets[hash].lock );
                return BASEADDROF(current_item, ht);
            }
#if defined(HELPFIRST)
            if( ht->hash(current_item->key, ht->rw_hash->size, ht->hash_data) == hash_main_bucket ) {
                /* It's not the target item, but it's an item that goes in the
                 * same bucket as the target item, so we already have the lock
                 * on that bucket in the main table: insert it there costs not
                 * much and would help getting rid of old tables */
                 if( NULL == prev_item ) {
                     head->buckets[hash].first_item = current_item->next_item;
                 } else {
                     prev_item->next_item = current_item->next_item;
                 }
                 res = parsec_atomic_dec_32b(&head->buckets[hash].cur_len);
                 if( 0 == res ) {
                     res = parsec_atomic_dec_32b(&head->used_buckets);
                     if( 0 == res ) {
                         parsec_atomic_cas_ptr(&prev_head->next, head, head->next);
                     }
                 }
                 parsec_hash_table_nolock_insert(ht, current_item);
                 if( NULL == prev_item )
                     current_item = head->buckets[hash].first_item;
                 else
                     current_item = prev_item->next_item;
            } else
#endif
            {
                prev_item = current_item;
                current_item = prev_item->next_item;
            }
        }
        parsec_atomic_unlock(&head->buckets[hash].lock );
        prev_head = head;
    }
    return NULL;
}

#if !defined(HELPFIRST)
static void *parsec_hash_table_nolock_find_in_old_tables(parsec_hash_table_t *ht, uint64_t key)
{
    parsec_hash_table_head_t *head;
    parsec_hash_table_item_t *current_item;
    uint32_t hash;
    for(head = ht->rw_hash->next; NULL != head; head = head->next) {
        hash = ht->hash(key, head->size, ht->hash_data);
        for(current_item = head->buckets[hash].first_item;
            NULL != current_item;
            current_item = current_item->next_item) {
            if( current_item->key == key ) {
                return BASEADDROF(current_item, ht);
            }
        }
    }
    return NULL;
}
#endif

void *parsec_hash_table_nolock_find(parsec_hash_table_t *ht, uint64_t key)
{
    parsec_hash_table_item_t *current_item;
    uint32_t hash;
    void *item;
    hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);
    for(current_item = ht->rw_hash->buckets[hash].first_item;
        NULL != current_item;
        current_item = current_item->next_item) {
        if( current_item->key == key ) {
            return BASEADDROF(current_item, ht);
        }
    }
#if defined(HELPFIRST)
    item = parsec_hash_table_nolock_remove_from_old_tables(ht, key);
    if( NULL != item ) {
        current_item = ITEMADDROF(item, ht);
        parsec_hash_table_nolock_insert(ht, current_item);
    }
#else
    item = parsec_hash_table_nolock_find_in_old_tables(ht, key);
#endif
    return item;
}

void *parsec_hash_table_nolock_remove(parsec_hash_table_t *ht, uint64_t key)
{
    parsec_hash_table_item_t *current_item, *prev_item;
    uint32_t hash;
    int32_t res;
    hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);
    prev_item = NULL;
    for(current_item = ht->rw_hash->buckets[hash].first_item;
        NULL != current_item;
        current_item = prev_item->next_item) {
        if( current_item->key == key ) {
            if( NULL == prev_item ) {
                ht->rw_hash->buckets[hash].first_item = current_item->next_item;
            } else {
                prev_item->next_item = current_item->next_item;
            }
            res = parsec_atomic_dec_32b(&ht->rw_hash->buckets[hash].cur_len);
            if( 0 == res ) {
                res = parsec_atomic_dec_32b(&ht->rw_hash->used_buckets);
            }
            return BASEADDROF(current_item, ht);
        }
        prev_item = current_item;
    }
    return parsec_hash_table_nolock_remove_from_old_tables(ht, key);
}

void parsec_hash_table_insert(parsec_hash_table_t *ht, parsec_hash_table_item_t *item)
{
    uint32_t hash;
    parsec_hash_table_head_t *cur_head;
    int resize;
    parsec_atomic_rwlock_rdlock(&ht->rw_lock);
    cur_head = ht->rw_hash;
    hash = ht->hash(item->key, ht->rw_hash->size, ht->hash_data);
    assert( hash < ht->rw_hash->size );
    parsec_atomic_lock(&ht->rw_hash->buckets[hash].lock);
    parsec_hash_table_nolock_insert(ht, item);
    resize = (ht->rw_hash->buckets[hash].cur_len > parsec_hash_table_max_collisions_hint);
    parsec_atomic_unlock(&ht->rw_hash->buckets[hash].lock);
    parsec_atomic_rwlock_rdunlock(&ht->rw_lock);
    
    if( resize ) {
        parsec_atomic_rwlock_wrlock(&ht->rw_lock);
        if( cur_head == ht->rw_hash ) {
            /* Barring ABA problems, nobody resized the hash table;
             * Good enough hint that it's our role to do so */
            parsec_hash_table_resize(ht);
        }
        /* Otherwise, let's asssume somebody resized already */
        parsec_atomic_rwlock_wrunlock(&ht->rw_lock);
    }
}

void *parsec_hash_table_find(parsec_hash_table_t *ht, uint64_t key)
{
    uint32_t hash;
    void *ret;
    parsec_atomic_rwlock_rdlock(&ht->rw_lock);
    hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);
    assert( hash < ht->rw_hash->size );
    parsec_atomic_lock(&ht->rw_hash->buckets[hash].lock);
    ret = parsec_hash_table_nolock_find(ht, key);
    parsec_atomic_unlock(&ht->rw_hash->buckets[hash].lock);
    parsec_atomic_rwlock_rdunlock(&ht->rw_lock);
    return ret;
}

void *parsec_hash_table_remove(parsec_hash_table_t *ht, uint64_t key)
{
    uint32_t hash;
    void *ret;
    parsec_atomic_rwlock_rdlock(&ht->rw_lock);
    hash = ht->hash(key, ht->rw_hash->size, ht->hash_data);
    assert( hash < ht->rw_hash->size );
    parsec_atomic_lock(&ht->rw_hash->buckets[hash].lock);
    ret = parsec_hash_table_nolock_remove(ht, key);
    parsec_atomic_unlock(&ht->rw_hash->buckets[hash].lock);
    parsec_atomic_rwlock_rdunlock(&ht->rw_lock);
    return ret;
}

void parsec_hash_table_stat(parsec_hash_table_t *ht)
{
    parsec_hash_table_head_t *head;
    double mean, M2, delta, delta2;
    int n, min, max;
    int nb;
    uint32_t i, j;
    parsec_hash_table_item_t *current_item;

    for(head = ht->rw_hash, j=0; NULL != head; head = head->next, j++) {
        n = 0;
        min = -1;
        max = -1;
        mean = 0.0;
        M2 = 0.0;
        for(i = 0; i < head->size; i++) {
            nb = 0;
            for(current_item = head->buckets[i].first_item;
                current_item != NULL;
                current_item = current_item->next_item) {
                nb++;
            }

            n++;
            delta = (double)nb - mean;
            mean += delta/n;
            delta2 = (double)nb - mean;
            M2 += delta*delta2;

            if( min == -1 || nb < min )
                min = nb;
            if( max == -1 || nb > max )
                max = nb;
        }
        printf("table %p level %d: %d lists, of length %d to %d average length: %g and variance %g\n",
               ht, j, n, min, max, mean, M2/(n-1));
    }
}

void parsec_hash_table_for_all(parsec_hash_table_t* ht, hash_elem_fct_t fct, void* cb_data)
{
    parsec_hash_table_head_t *head;
    parsec_hash_table_item_t *current_item;
    void* user_item;

    for( head = ht->rw_hash; NULL != head; head = head->next ) {
        for( size_t i = 0; i < head->size; i++ ) {
            current_item = head->buckets[i].first_item;
            /* Iterating the list to check if we have the element */
            while( NULL != current_item ) {
                user_item = parsec_hash_table_item_lookup(ht, current_item);
                current_item = current_item->next_item;
                fct( cb_data, user_item );
            }
        }
    }
}
