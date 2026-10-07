#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "pointer_table.h"

struct PtNode {
	struct PointerTable *table;
	uint32_t key;
	uint32_t type;
	uint32_t refs;
	uint32_t user_refs;
	pointer_table_destroy_fn destroy_fn;
	void *payload;
	void *pending_destroy_arg;
};

struct PtSlot {
	uint32_t key;
	struct PtNode *node;
};

struct PointerTable {
	struct PtSlot *slots;
	uint32_t cap;
	uint32_t count;
	uint32_t dead;
	pthread_mutex_t lock;
	uint32_t next_key;
};

static uint32_t pt_hash(uint32_t key) { return key * 0x9E3779B9u; }

static void pt_node_destroy(struct PtNode *node) {
	if (node->destroy_fn)
		node->destroy_fn(node->payload, node->user_refs, node->pending_destroy_arg);
	free(node);
}

static struct PtSlot *pt_find_slot_locked(struct PointerTable *table, uint32_t key) {
	uint32_t mask = table->cap - 1;
	uint32_t i = pt_hash(key) & mask;

	while (table->slots[i].key != 0) {
		if (table->slots[i].key == key && table->slots[i].node)
			return &table->slots[i];
		i = (i + 1) & mask;
	}

	return NULL;
}

static bool pt_insert_slot_locked(struct PointerTable *table, uint32_t key, struct PtNode *node) {
	uint32_t mask = table->cap - 1;
	uint32_t i = pt_hash(key) & mask;
	struct PtSlot *first_dead = NULL;

	for (;;) {
		struct PtSlot *slot = &table->slots[i];

		if (slot->key == 0) {
			if (first_dead) {
				slot = first_dead;
				table->dead--;
			}
			slot->key = key;
			slot->node = node;
			table->count++;
			return true;
		}

		if (slot->node && slot->key == key)
			return false;

		if (!slot->node && !first_dead)
			first_dead = slot;

		i = (i + 1) & mask;
	}
}

static bool pt_grow_locked(struct PointerTable *table) {
	struct PtSlot *old = table->slots;
	uint32_t old_cap = table->cap;
	uint32_t new_cap = old_cap * 2;
	struct PtSlot *slots = calloc(new_cap, sizeof(*slots));
	uint32_t i;

	if (!slots) return false;

	table->slots = slots;
	table->cap = new_cap;
	table->dead = 0;

	for (i = 0; i < old_cap; i++) {
		if (old[i].node) {
			uint32_t mask = new_cap - 1;
			uint32_t j = pt_hash(old[i].key) & mask;

			while (slots[j].key != 0)
				j = (j + 1) & mask;
			slots[j] = old[i];
		}
	}

	free(old);
	return true;
}

static struct PtNode *pt_detach_locked(struct PointerTable *table, uint32_t key, void *destroy_arg,
                                        bool *p_destroy_now) {
	struct PtSlot *slot = pt_find_slot_locked(table, key);
	struct PtNode *node;

	if (!slot) return NULL;

	node = slot->node;
	slot->node = NULL;
	table->count--;
	table->dead++;
	node->pending_destroy_arg = destroy_arg;
	node->refs--;

	*p_destroy_now = node->refs == 0;

	return node;
}

struct PointerTable *pointer_table_create(void) {
	struct PointerTable *table = malloc(sizeof(*table));

	if (!table) return NULL;

	memset(table, 0, sizeof(*table));

	if (pthread_mutex_init(&table->lock, NULL) != 0) {
		free(table);
		return NULL;
	}

	table->slots = calloc(8, sizeof(*table->slots));
	if (!table->slots) {
		pthread_mutex_destroy(&table->lock);
		free(table);
		return NULL;
	}
	table->cap = 8;
	table->next_key = 1;

	return table;
}

void pointer_table_destroy(struct PointerTable *table, void *destroy_arg) {
	uint32_t i;
	if (!table) return;

	pthread_mutex_lock(&table->lock);
	for (i = 0; i < table->cap; i++) {
		struct PtNode *node = table->slots[i].node;
		if (!node) continue;

		table->slots[i].node = NULL;
		node->pending_destroy_arg = destroy_arg;
		node->refs--;
		if (node->refs == 0)
			pt_node_destroy(node);
	}
	pthread_mutex_unlock(&table->lock);

	free(table->slots);
	pthread_mutex_destroy(&table->lock);
	free(table);
}

struct PtNode *pointer_table_acquire(struct PointerTable *table, uint32_t key, uint32_t type) {
	struct PtSlot *slot;
	struct PtNode *node = NULL;
	if (!table) return NULL;

	pthread_mutex_lock(&table->lock);
	slot = pt_find_slot_locked(table, key);
	if (slot && slot->node->type == type) {
		node = slot->node;
		node->refs++;
	}
	pthread_mutex_unlock(&table->lock);

	return node;
}

void *pointer_table_payload(struct PtNode *entry) { return entry ? entry->payload : NULL; }

void pointer_table_release(struct PtNode *entry) {
	struct PointerTable *table;
	uint32_t refs;
	if (!entry) return;

	table = entry->table;

	pthread_mutex_lock(&table->lock);
	refs = --entry->refs;
	pthread_mutex_unlock(&table->lock);

	if (refs == 0)
		pt_node_destroy(entry);
}

uint32_t pointer_table_add_with_key(struct PointerTable *table, uint32_t key, uint32_t type,
                                    pointer_table_destroy_fn destroy_fn, void *payload) {
	struct PtNode *node;
	if (!table || key == 0) return 0;
	if (!(node = malloc(sizeof(*node)))) return 0;

	memset(node, 0, sizeof(*node));
	node->table = table;
	node->key = key;
	node->type = type;
	node->refs = 1;
	node->user_refs = 1;
	node->destroy_fn = destroy_fn;
	node->payload = payload;

	pthread_mutex_lock(&table->lock);
	if (table->count + table->dead + 1 > table->cap * 3 / 4 && !pt_grow_locked(table)) {
		pthread_mutex_unlock(&table->lock);
		free(node);
		return 0;
	}
	if (!pt_insert_slot_locked(table, key, node)) {
		pthread_mutex_unlock(&table->lock);
		free(node);
		return 0;
	}
	pthread_mutex_unlock(&table->lock);

	return key;
}

uint32_t pointer_table_add(struct PointerTable *table, uint32_t type, pointer_table_destroy_fn destroy_fn, void *payload) {
	uint32_t key = 0;

	if (!table) return 0;

	pthread_mutex_lock(&table->lock);
	do {
		key = table->next_key++;
		if (table->next_key == 0)
			table->next_key = 1;
	} while (pt_find_slot_locked(table, key));
	pthread_mutex_unlock(&table->lock);

	return pointer_table_add_with_key(table, key, type, destroy_fn, payload);
}

uint32_t pointer_table_user_ref(struct PointerTable *table, uint32_t key) {
	struct PtSlot *slot;
	uint32_t n = 0;

	if (!table) return 0;

	pthread_mutex_lock(&table->lock);
	slot = pt_find_slot_locked(table, key);
	if (slot)
		n = ++slot->node->user_refs;
	pthread_mutex_unlock(&table->lock);

	return n;
}

uint32_t pointer_table_user_unref(struct PointerTable *table, uint32_t key, void *destroy_arg) {
	struct PtSlot *slot;
	struct PtNode *doomed = NULL;
	uint32_t prev = 0;

	if (!table) return 0;

	pthread_mutex_lock(&table->lock);
	slot = pt_find_slot_locked(table, key);
	if (slot && slot->node->user_refs > 0) {
		struct PtNode *node = slot->node;

		prev = node->user_refs;
		node->user_refs--;
		if (node->user_refs == 0) {
			node->pending_destroy_arg = destroy_arg;
			slot->node = NULL;
			table->count--;
			table->dead++;
			node->refs--;
			doomed = node->refs == 0 ? node : NULL;
		}
	}
	pthread_mutex_unlock(&table->lock);

	if (doomed) pt_node_destroy(doomed);

	return prev;
}

bool pointer_table_remove(struct PointerTable *table, uint32_t key, void *destroy_arg) {
	bool destroy_now = false;
	struct PtNode *doomed;

	if (!table) return false;

	pthread_mutex_lock(&table->lock);
	doomed = pt_detach_locked(table, key, destroy_arg, &destroy_now);
	pthread_mutex_unlock(&table->lock);

	if (doomed && destroy_now)
		pt_node_destroy(doomed);

	return doomed != NULL;
}
