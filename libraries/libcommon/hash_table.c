#include "hash_table.h"

#include <stddef.h>
#include <libz/memory.h>
#include <libz/malloc.h>

#define OFFSET_BASIS 14695981039346656037ULL
#define PRIME 1099511628211ULL

static uint8_t is_power_of_2(uint64_t value) {
    return (value & (value - 1)) == 0;
}

int hash_table_init(hash_table_t *hash_table, uint64_t bucket_count) {
    if (hash_table == NULL || bucket_count == 0) {
        return -1;
    }

    // Make sure the bucket count is a power of 2.
    if (!is_power_of_2(bucket_count)) {
        return -1;
    }

    hash_table->buckets = (hash_node_t **)malloc(bucket_count * sizeof(hash_node_t *));
    if (hash_table->buckets == NULL) {
        return -1;
    }

    for (uint64_t i = 0; i < bucket_count; i++) {
        hash_table->buckets[i] = NULL;
    }
    hash_table->bucket_count = bucket_count;
    hash_table->node_count = 0;

    return 0;
}

int hash_table_destroy(hash_table_t *hash_table) {
    if (hash_table == NULL) {
        return -1;
    }

    for (uint64_t i = 0; i < hash_table->bucket_count; i++) {
        if (hash_table->buckets[i] == NULL) {
            continue;
        }

        hash_node_t *node = hash_table->buckets[i];
        while (node != NULL) {
            hash_node_t *next_node = node->next;
            free(node);
            node = next_node;
        }
        hash_table->buckets[i] = NULL;
    }

    free(hash_table->buckets);
    hash_table->buckets = NULL;
    hash_table->bucket_count = 0;
    hash_table->node_count = 0;

    return 0;
}

// FNV-1a 64-bit hash function.
static uint64_t hash_function(void *data, uint64_t size) {
    uint64_t hash = OFFSET_BASIS;
    for (uint64_t i = 0; i < size; i++) {
        hash = (hash ^ ((uint8_t*)data)[i]) * PRIME;
    }
    return hash;
}

static uint64_t hash_key_to_index(hash_table_t *hash_table, hash_key_t *key) {
    uint64_t hash_value = 0;
    if (key->size == 0) {
        uint64_t value = key->u64;
        hash_value = hash_function(&value, sizeof(uint64_t));
    } else {
        hash_value = hash_function(key->data, key->size);
    }

    return hash_value & (hash_table->bucket_count - 1);
}

static uint8_t key_matches(hash_node_t *node, hash_key_t *key) {
    if (node->key.size == 0) {
        return node->key.u64 == key->u64;
    }
    return node->key.size == key->size && memory_compare(node->key.data, key->data, node->key.size) == 0;
}

static hash_node_t *find_node_by_bucket(hash_table_t *hash_table, uint64_t bucket_idx, hash_key_t *key) {
    hash_node_t *node = hash_table->buckets[bucket_idx];
    while (node != NULL) {
        if (key_matches(node, key)) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

static hash_node_t *find_node(hash_table_t *hash_table, hash_key_t *key) {
    if (hash_table == NULL || key == NULL) {
        return NULL;
    }

    uint64_t index = hash_key_to_index(hash_table, key);
    return find_node_by_bucket(hash_table, index, key);
}

int hash_table_get(hash_table_t *hash_table, hash_key_t *key, void **data) {
    if (hash_table == NULL || key == NULL || data == NULL) {
        return -1;
    }

    hash_node_t *node = find_node(hash_table, key);
    if (node == NULL) {
        return -1;
    }

    *data = node->data;
    return 0;
}

int hash_table_set(hash_table_t *hash_table, hash_key_t *key, void *data) {
    if (hash_table == NULL || key == NULL || data == NULL) {
        return -1;
    }

    uint64_t index = hash_key_to_index(hash_table, key);
    hash_node_t *node = find_node_by_bucket(hash_table, index, key);

    if (node == NULL) {
        node = (hash_node_t *)malloc(sizeof(hash_node_t));
        if (node == NULL) {
            return -1;
        }

        node->key = *key;
        node->next = NULL;
        node->prev = NULL;

        hash_node_t *list_head = hash_table->buckets[index];
        if (list_head != NULL) {
            node->next = list_head;
            list_head->prev = node;
        }
        hash_table->buckets[index] = node;
        hash_table->node_count++;
    }
    
    node->data = data;
    return 0;
}

int hash_table_remove(hash_table_t *hash_table, hash_key_t *key) {
    if (hash_table == NULL || key == NULL) {
        return -1;
    }

    uint64_t index = hash_key_to_index(hash_table, key);
    hash_node_t *node = find_node_by_bucket(hash_table, index, key);
    if (node == NULL) {
        return -1;
    }

    hash_node_t *list_head = hash_table->buckets[index];
    if (list_head == node) {
        hash_table->buckets[index] = node->next;
        node->next->prev = NULL;
    } else {
        // Node behind us? Link it to the node in front of us.
        if (node->prev != NULL) {
            node->prev->next = node->next;
        }
        // Node in front of us? Link it to the node behind us.
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }
    }

    free(node);
    return 0;
}

uint8_t hash_table_contains(hash_table_t *hash_table, hash_key_t* key) {
    return find_node(hash_table, key) != NULL;
}

hash_key_t hash_key_create_data(void *data, uint64_t size) {
    hash_key_t key;
    key.data = data;
    key.size = size;
    return key;
}

hash_key_t hash_key_create_u64(uint64_t value) {
    hash_key_t key;
    key.u64 = value;
    key.size = 0; // Zero to indicate a u64 key.
    return key;
}
