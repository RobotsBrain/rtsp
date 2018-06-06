/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/* hashtable.h: Open addressing hashtable interface */
#ifndef HASHTABLE_
#define HASHTABLE_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
	OK,
	ERR,
	MINSIZE
} Hashstatus;

typedef struct _hashtable hashtable;

typedef unsigned long (*hashfunc) (void *);
typedef int (*cmpfunc) (void *, void *);


unsigned long longhash(void *n);


int longequal(void *a, void *b);


hashtable* hash_table_create(hashfunc hfun, cmpfunc cfun, unsigned long initsize, char freeelements);


void freehashtable(hashtable **ht);


void clearhashtable(hashtable **ht);


Hashstatus puthashtable(hashtable **ht, void *key, void *value);


void* gethashtable(hashtable **ht, void *key);


Hashstatus delhashtable(hashtable **ht, void *key);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*HASHTABLE_*/
