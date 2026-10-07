// Guest-safe pointer registry: integer keys mapped to host pointers.
//
// A native wrapper never exposes a real host pointer to guest code. Instead
// every host object (context, device, open handle, descriptor blob, ...) is
// registered in a pointer_table and the guest only ever sees the integer
// key, typed as an opaque pointer in the wrapper's guest-side API.
//
// Entry model:
// - every entry carries a caller-defined type tag; lookups validate the tag,
//   so a key of the wrong type is simply "not found"
// - entries are created with user_refs = 1, the guest's C-level reference;
//   when user_refs drops to 0 (pointer_table_user_unref) the entry is
//   removed and its destroy callback runs
// - pointer_table_remove() removes an entry regardless of its refcount
// - pointer_table_acquire() refcounts an entry so it stays valid until
//   pointer_table_release(), even if it is removed or user-unref'd in
//   between
// - key 0 is reserved (used internally as the empty-slot marker)
// - keys are either allocated by the table (pointer_table_add) or supplied
//   by the caller (pointer_table_add_with_key, e.g. keyed by a guest heap
//   address)
//
// Storage: open-addressing hash table, linear probing, power-of-two
// capacity. A removed entry is tombstoned (its key stays so probe chains
// stay connected) and its slot is recycled by later inserts. The slot index
// is the key multiplied by the golden ratio constant (Fibonacci hashing):
// guest heap addresses, used as keys, are 4/8 byte aligned and would
// otherwise cluster into a fraction of the slots. When live entries plus
// tombstones exceed three quarters of the capacity the table doubles,
// reinserting the live entries and dropping the tombstones.
//
// The implementation is fully self-contained (std C + pthread, no runtime
// or OS dependencies), so the table can also be used outside of a WAMR
// embedder. The typical WAMR integration stores the table in the module
// instance context (wasm_runtime_create_context_key() and friends): one
// table per instance/cluster, automatically destroyed when the main
// instance is deinstantiated.
//
// Threading: one mutex per table; all operations are serialized on it.
// Destroy callbacks run while the table lock is held and must not re-enter
// the table.

#ifndef _POINTER_TABLE_H
#define _POINTER_TABLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PointerTable;

// Opaque reference to one acquired entry; valid until pointer_table_release().
struct PtNode;

// Destroy callback of a single entry: payload is the pointer given to
// pointer_table_add[_with_key](), user_refs_left the guest references that
// were never released (0 on the normal path; > 0 when the table is destroyed
// while the guest still holds references), destroy_arg the opaque argument
// of the call that triggered destruction.
typedef void (*pointer_table_destroy_fn)(void *payload, uint32_t user_refs_left, void *destroy_arg);

// Create a registry with a small initial capacity; the storage grows
// automatically as entries are added. Returns the registry, NULL on failure.
struct PointerTable *pointer_table_create(void);

// Destroy a registry and every entry still in it. Entries with outstanding
// pointer_table_acquire() references are not destroyed until the last
// reference is released (their destroy callback then runs with this call's
// destroy_arg). Every entry acquired from the table must be released before
// the table itself is destroyed. destroy_arg is forwarded to every destroy
// callback.
void pointer_table_destroy(struct PointerTable *table, void *destroy_arg);

// Add an entry with an automatically allocated key; the entry starts with
// user_refs = 1. type is a caller-defined tag used to validate lookups,
// destroy_fn a cleanup callback (NULL = no cleanup beyond the node), payload
// the host pointer to store. Returns the key, or 0 on failure.
uint32_t pointer_table_add(struct PointerTable *table, uint32_t type, pointer_table_destroy_fn destroy_fn, void *payload);

// Add an entry with a caller-supplied key (e.g. a guest heap address).
// Returns the key, or 0 on failure (key 0 or key already in use).
uint32_t pointer_table_add_with_key(struct PointerTable *table, uint32_t key, uint32_t type, pointer_table_destroy_fn destroy_fn, void *payload);

// Acquire an entry: its payload stays valid until pointer_table_release(),
// even if the entry is removed or user-unref'd in between. Returns an
// opaque entry reference, NULL if the key is unknown or has the wrong type.
struct PtNode *pointer_table_acquire(struct PointerTable *table, uint32_t key, uint32_t type);

void *pointer_table_payload(struct PtNode *entry);

// Release an entry acquired with pointer_table_acquire().
void pointer_table_release(struct PtNode *entry);

// Add a guest-level reference. Returns the new user refcount, or 0 if the
// key is unknown.
uint32_t pointer_table_user_ref(struct PointerTable *table, uint32_t key);

// Drop a guest-level reference. When the refcount reaches 0 the entry is
// removed and its destroy callback scheduled (immediately, or after the
// last outstanding pointer_table_release()). Returns the refcount before
// the call, 0 if the key is unknown or the refcount was already 0 (no-op).
uint32_t pointer_table_user_unref(struct PointerTable *table, uint32_t key, void *destroy_arg);

// Remove an entry regardless of its refcount. Returns true if the entry
// existed, false otherwise.
bool pointer_table_remove(struct PointerTable *table, uint32_t key, void *destroy_arg);

#ifdef __cplusplus
}
#endif

#endif
