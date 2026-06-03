#pragma once

#include <stdint.h>

typedef struct hash_key_t {
    union {
        uint64_t u64;
        void *data;
    };
    uint64_t size;
} hash_key_t;

typedef struct hash_node_t {
    hash_key_t key;
    void *data;
    struct hash_node_t* next;
    struct hash_node_t* prev;
} hash_node_t;

typedef struct hash_table_t {
    hash_node_t **buckets;
    uint64_t bucket_count;
    uint64_t node_count;
} hash_table_t;

int hash_table_init(hash_table_t *hash_table, uint64_t bucket_count);
int hash_table_destroy(hash_table_t *hash_table);

int hash_table_get(hash_table_t *hash_table, hash_key_t *key, void **data);
int hash_table_set(hash_table_t *hash_table, hash_key_t *key, void *data);
int hash_table_remove(hash_table_t *hash_table, hash_key_t *key);
uint8_t hash_table_contains(hash_table_t *hash_table, hash_key_t* key);

hash_key_t hash_key_create_data(void *data, uint64_t size);
hash_key_t hash_key_create_u64(uint64_t value);
