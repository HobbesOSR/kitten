/*
  Copyright (c) 2002, 2004, Christopher Clark
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  
  * Neither the name of the original author; nor the names of any contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.
  
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#include <lwk/types.h>
#include <lwk/kmem.h>
#include <lwk/spinlock.h>
#include <arch/string.h>

#include <xpmem_hashtable.h>


struct hash_entry {
    uintptr_t		key;
    uintptr_t		value;
    u32			hash;
    struct hash_entry * next;
};

struct xpmem_hashtable {
    struct hash_entry ** table;
    u32			 table_length;
    u32			 entry_count;
    u32			 load_limit;
    u32			 prime_index;
    u32 (*hash_fn) (uintptr_t key);
    int (*eq_fn) (uintptr_t key1, uintptr_t key2);

    spinlock_t		 lock;
    int			 expanding;
};


/* HASH FUNCTIONS */

static inline u32 
do_hash(struct xpmem_hashtable * htable, 
	uintptr_t		 key)
{
    /* Aim to protect against poor hash functions by adding logic here
     * - logic taken from java 1.4 hashtable source */

    u32 i = htable->hash_fn(key);
    i += ~(i << 9);
    i ^=  ((i >> 14) | (i << 18)); /* >>> */
    i +=  (i << 4);
    i ^=  ((i >> 10) | (i << 22)); /* >>> */

    return i;
}


/* HASH AN UNSIGNED LONG */
/* LINUX UNSIGHED LONG HASH FUNCTION */

/*  2^63 + 2^61 - 2^57 + 2^54 - 2^51 - 2^18 + 1 */
#define GOLDEN_RATIO_PRIME 0x9e37fffffffc0001UL
#define BITS_PER_LONG 64


u32 
hash_long(u64 val, 
	  u32 bits)
{
    u64 hash = val;


    /*	Sigh, gcc can't optimise this alone like it does for 32 bits. */
    u64 n = hash;
    n <<= 18;
    hash -= n;
    n <<= 33;
    hash -= n;
    n <<= 3;
    hash += n;
    n <<= 3;
    hash -= n;
    n <<= 4;
    hash += n;
    n <<= 2;
    hash += n;

    /* High bits are more random, so use them. */
    return hash >> (BITS_PER_LONG - bits);
}

/* HASH GENERIC MEMORY BUFFER */
/* ELF HEADER HASH FUNCTION */
u32 
hash_buffer(u8 * msg, 
	    u32  length)
{
    u32 hash = 0;
    u32 temp = 0;
    u32 i;

    for (i = 0; i < length; i++) {
	hash = (hash << 4) + *(msg + i) + i;
	if ((temp = (hash & 0xF0000000))) {
	    hash ^= (temp >> 24);
	}
	hash &= ~temp;
    }
    return hash;
}

/* indexFor */
static inline u32 
indexFor(u32 table_length, 
	 u32 hash_value) 
{
    return (hash_value % table_length);
};

#define freekey(X) kmem_free(X)


/*
  Credit for primes table: Aaron Krowne
  http://br.endernet.org/~akrowne/
  http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
*/
static const u32 primes[] = { 
    53, 97, 193, 389,
    769, 1543, 3079, 6151,
    12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869,
    3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189,
    805306457, 1610612741 };


// this assumes that the max load factor is .65
static const u32 load_factors[] = {
    35, 64, 126, 253,
    500, 1003, 2002, 3999,
    7988, 15986, 31953, 63907,
    127799, 255607, 511182, 1022365,
    2044731, 4089455, 8178897, 16357798,
    32715575, 65431158, 130862298, 261724573,
    523449198, 1046898282 };

const u32 prime_table_len = sizeof(primes) / sizeof(primes[0]);

struct xpmem_hashtable * create_htable(u32 min_size,
				    u32 (*hash_fn) (uintptr_t),
				    int (*eq_fn) (uintptr_t, uintptr_t)) {
    struct xpmem_hashtable * htable;
    u32 prime_index;
    u32 size = primes[0];

    /* Check requested hashtable isn't too large */
    if (min_size > (1u << 30)) {
	return NULL;
    }

    /* Enforce size as prime */
    for (prime_index = 0; prime_index < prime_table_len; prime_index++) {
	if (primes[prime_index] > min_size) { 
	    size = primes[prime_index]; 
	    break; 
	}
    }

    htable = (struct xpmem_hashtable *)kmem_alloc(sizeof(struct xpmem_hashtable));

    if (htable == NULL) {
	return NULL; /*oom*/
    }

    htable->table = (struct hash_entry **)kmem_alloc(sizeof(struct hash_entry*) * size);

    if (htable->table == NULL) { 
	kmem_free(htable); 
	return NULL;  /*oom*/
    }

    memset(htable->table, 0, size * sizeof(struct hash_entry *));

    htable->table_length  = size;
    htable->prime_index   = prime_index;
    htable->entry_count   = 0;
    htable->hash_fn	  = hash_fn;
    htable->eq_fn	  = eq_fn;
    htable->load_limit	  = load_factors[prime_index];
    htable->expanding	  = 0;
    
    spin_lock_init(&(htable->lock));

    return htable;
}


static int 
hashtable_expand(struct xpmem_hashtable * htable,
		 struct hash_entry     ** new_table,
		 u32			  new_size) 
{
    /* Double the size of the table to accomodate more entries */
    struct hash_entry * tmp_entry = NULL;
    u32 i = 0;
    u32 index = 0;

    memset(new_table, 0, new_size * sizeof(struct hash_entry *));
    /* This algorithm is not 'stable'. ie. it reverses the list
    * when it transfers entries between the tables */

    for (i = 0; i < htable->table_length; i++) {

	while ((tmp_entry = htable->table[i]) != NULL) {
	    htable->table[i] = tmp_entry->next;

	    index = indexFor(new_size, tmp_entry->hash);

	    tmp_entry->next = new_table[index];

	    new_table[index] = tmp_entry;
	}
    }

    kmem_free(htable->table);

    htable->table	 = new_table;
    htable->table_length = new_size;
    htable->load_limit	 = load_factors[htable->prime_index];

    return -1;
}

u32 
htable_count(struct xpmem_hashtable * htable)
{
    return htable->entry_count;
}

int 
htable_insert(struct xpmem_hashtable * htable, uintptr_t key, uintptr_t value) {
    /* This method allows duplicate keys - but they shouldn't be used */
    struct hash_entry ** new_table = NULL;
    struct hash_entry * new_entry = NULL;
    u32 index = 0;
    u32 new_size = 0;
    int expand = 0;

    /* Allocate new entry */
    new_entry = (struct hash_entry *)kmem_alloc(sizeof(struct hash_entry));
    if (new_entry == NULL) {
	return 0; /* oom */
    }
    
    /* Lock htable */
    spin_lock(&(htable->lock));

    /* Determine if table needs expanding */
    if ((htable->expanding	  == 0) &&
	(htable->entry_count + 1)  > htable->load_limit)
    {
	expand		  = 1;
	htable->expanding = 1;
    }

    /* Increment entry count */
    ++(htable->entry_count);

    if (expand) {
	/* Check we're not hitting max capacity */
	if (htable->prime_index == (prime_table_len - 1)) {
	    kmem_free(new_entry);
	    htable->expanding = 0;
	    spin_unlock(&(htable->lock));
	    return 0;
	}

	new_size = primes[++(htable->prime_index)];
	
	/* Drop lock just to allocate memory. There's a race between re-acquiring the lock
	 * after allocation and another process skipping over the expansion block and
	 * inserting, but expansion is not strictly required for insertion so the race is
	 * inconsequential
	 */
	spin_unlock(&(htable->lock));
	new_table = (struct hash_entry **)kmem_alloc(sizeof(struct hash_entry *) * new_size);
	spin_lock(&(htable->lock));

	if (new_table == NULL) {
	    (htable->prime_index)--;
	    kmem_free(new_entry);
	    htable->expanding = 0;
	    spin_unlock(&(htable->lock));
	    return 0; /* oom */
	}

	/* Ignore the return value. If expand fails, we should
	 * still try cramming just this value into the existing table
	 * -- we may not have memory for a larger table, but one more
	 * element may be ok. Next time we insert, we'll try expanding again.*/
	hashtable_expand(htable, new_table, new_size);
    }

    new_entry->hash = do_hash(htable, key);

    index = indexFor(htable->table_length, new_entry->hash);

    new_entry->key = key;
    new_entry->value = value;

    new_entry->next = htable->table[index];

    htable->table[index] = new_entry;

    /* Unlock htable */
    spin_unlock(&(htable->lock));

    return -1;
}


static int 
__htable_change(struct xpmem_hashtable * htable, 
		uintptr_t		 key, 
		uintptr_t		 value, 
		int			 free_value)
{
    struct hash_entry * tmp_entry;
    u32 hash_value;
    u32 index;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    tmp_entry = htable->table[index];

    while (tmp_entry != NULL) {
	/* Check hash value to short circuit heavier comparison */
	if ((hash_value == tmp_entry->hash) && (htable->eq_fn(key, tmp_entry->key))) {

	    if (free_value) {
		kmem_free((void *)(tmp_entry->value));
	    }

	    tmp_entry->value = value;
	    return -1;
	}
	tmp_entry = tmp_entry->next;
    }
    return 0;
}

int
htable_change(struct xpmem_hashtable * htable,
	      uintptr_t		       key,
	      uintptr_t		       value,
	      int		       free_value)
{
    int ret = 0;

    spin_lock(&(htable->lock));
    {
	ret = __htable_change(htable, key, value, free_value);
    }
    spin_unlock(&(htable->lock));

    return ret;
}



static int 
__htable_inc(struct xpmem_hashtable * htable, 
	     uintptr_t		      key, 
	     uintptr_t		      value)
{
    struct hash_entry * tmp_entry = NULL;
    u32 hash_value = 0;
    u32 index = 0;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    tmp_entry = htable->table[index];

    while (tmp_entry != NULL) {
	/* Check hash value to short circuit heavier comparison */
	if ((hash_value == tmp_entry->hash) && (htable->eq_fn(key, tmp_entry->key))) {

	    tmp_entry->value += value;
	    return -1;
	}
	tmp_entry = tmp_entry->next;
    }
    return 0;
}

int
htable_inc(struct xpmem_hashtable * htable, 
	   uintptr_t		    key, 
	   uintptr_t		    value)
{
    int ret = 0;

    spin_lock(&(htable->lock));
    {
	ret = __htable_inc(htable, key, value);
    }
    spin_unlock(&(htable->lock));

    return ret;
}



static int 
__htable_dec(struct xpmem_hashtable * htable, 
	     uintptr_t		      key, 
	     uintptr_t		      value)
{
    struct hash_entry * tmp_entry = NULL;
    u32 hash_value = 0;
    u32 index = 0;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    tmp_entry = htable->table[index];

    while (tmp_entry != NULL) {
	/* Check hash value to short circuit heavier comparison */
	if ((hash_value == tmp_entry->hash) && (htable->eq_fn(key, tmp_entry->key))) {

	    tmp_entry->value -= value;
	    return -1;
	}
	tmp_entry = tmp_entry->next;
    }
    return 0;
}

int 
htable_dec(struct xpmem_hashtable * htable, 
	   uintptr_t		    key, 
	   uintptr_t		    value)
{
    int ret = 0;

    spin_lock(&(htable->lock));
    {
	ret = __htable_dec(htable, key, value);
    }
    spin_unlock(&(htable->lock));

    return ret;
}



/* returns value associated with key */
static uintptr_t 
__htable_search(struct xpmem_hashtable * htable, 
		uintptr_t		 key)
{
    struct hash_entry * cursor = NULL;
    u32 hash_value = 0;
    u32 index = 0;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    cursor = htable->table[index];

    while (cursor != NULL) {
	/* Check hash value to short circuit heavier comparison */
	if ((hash_value == cursor->hash) && 
		(htable->eq_fn(key, cursor->key))) {
	    return cursor->value;
	}

	cursor = cursor->next;
    }

    return (uintptr_t)NULL;
}

uintptr_t 
htable_search(struct xpmem_hashtable * htable, 
	      uintptr_t		       key)
{
    uintptr_t ret = 0;

    spin_lock(&(htable->lock));
    {
	ret = __htable_search(htable, key);
    }
    spin_unlock(&(htable->lock));

    return ret;
}



/* returns value associated with key */
static uintptr_t 
__htable_remove(struct xpmem_hashtable * htable, 
		uintptr_t		 key, 
		int			 free_key)
{
    /* TODO: consider compacting the table when the load factor drops enough,
     *	     or provide a 'compact' method. */

    struct hash_entry * cursor = NULL;
    struct hash_entry ** entry_ptr = NULL;
    uintptr_t value = 0;
    u32 hash_value = 0;
    u32 index = 0;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    entry_ptr = &(htable->table[index]);
    cursor = *entry_ptr;

    while (cursor != NULL) {
	/* Check hash value to short circuit heavier comparison */
	if ((hash_value == cursor->hash) && 
		(htable->eq_fn(key, cursor->key))) {

	    *entry_ptr = cursor->next;
	    htable->entry_count--;
	    value = cursor->value;

	    if (free_key) {
		freekey((void *)(cursor->key));
	    }
	    kmem_free(cursor);

	    return value;
	}

	entry_ptr = &(cursor->next);
	cursor = cursor->next;
    }
    return (uintptr_t)NULL;
}

uintptr_t 
htable_remove(struct xpmem_hashtable * htable, 
	      uintptr_t		       key, 
	      int		       free_key)
{
    uintptr_t ret = 0;

    spin_lock(&(htable->lock));
    {
	ret = __htable_remove(htable, key, free_key);
    }
    spin_unlock(&(htable->lock));

    return ret;
}



/* destroy */
static void 
__free_htable(struct xpmem_hashtable * htable, 
	      int		       free_values, 
	      int		       free_keys)
{
    u32 i;
    struct hash_entry * cursor = NULL;
    struct hash_entry * tmp = NULL;
    struct hash_entry ** table = htable->table;

    if (free_values) {
	for (i = 0; i < htable->table_length; i++) {
	    cursor = table[i];

	    while (cursor != NULL) { 
		tmp = cursor; 
		cursor = cursor->next; 

		if (free_keys) {
		    freekey((void *)(tmp->key)); 
		}
		kmem_free((void *)(tmp->value)); 
		kmem_free(tmp); 
	    }
	}
    } else {
	for (i = 0; i < htable->table_length; i++) {
	    cursor = table[i];

	    while (cursor != NULL) { 
		struct hash_entry * tmp = NULL;

		tmp = cursor; 
		cursor = cursor->next; 

		if (free_keys) {
		    freekey((void *)(tmp->key)); 
		}
		kmem_free(tmp); 
	    }
	}
    }

    kmem_free(htable->table);
}


void 
free_htable(struct xpmem_hashtable * htable, 
	    int			     free_values, 
	    int			     free_keys)
{
    spin_lock(&(htable->lock));
    {
	__free_htable(htable, free_values, free_keys);
    }
    spin_unlock(&(htable->lock));

    kmem_free(htable);
}
