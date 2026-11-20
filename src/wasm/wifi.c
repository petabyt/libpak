/* Guest-visible Wi-Fi objects are wasm32 snapshots backed by typed native
 * handles. Each AP retains its adapter until the AP itself is released. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <wasm_export.h>
extern int wasm_runtime_dup_wasi_socket(wasm_module_inst_t inst, uint32_t fd);
#include <wifi.h>
#include "main.h"
#include "layout.h"
#include "memory.h"
#include "pointer_table.h"

enum { WIFI_ADAPTER = 1, WIFI_AP };
struct wifi_state;

struct wifi_object {
	struct wifi_state *state;
	struct wifi_object *next;
	void *native;
	uint32_t guest, type, parent_guest;
	struct PtNode *parent;
};

struct wifi_state {
	struct wifi_state *next;
	wasm_module_inst_t inst;
	struct PakNet *net;
	int owned;
	struct PointerTable *table;
	struct wifi_object *objects;
};
static struct wifi_state *states;

static struct wifi_state *state_for(wasm_exec_env_t env) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	for (struct wifi_state *s = states; s; s = s->next)
		if (s->inst == inst)
			return s;
	return NULL;
}

bool wasm_wifi_attach(wasm_module_inst_t inst, struct PakNet *net) {
	struct wifi_state *s = calloc(1, sizeof(*s));
	if (!s)
		return false;
	s->table = pointer_table_create();
	if (!s->table) {
		free(s);
		return false;
	}
	s->inst = inst;
	s->net = net;
	s->next = states;
	states = s;
	return true;
}

static struct wifi_state *check(wasm_exec_env_t env, uint32_t token) {
	struct wifi_state *s = state_for(env);
	return s && token == 1 && s->net ? s : NULL;
}

static struct PtNode *acquire(struct wifi_state *s, uint32_t key, uint32_t type) {
	return s ? pointer_table_acquire(s->table, key, type) : NULL;
}

static void destroy_object(void *payload, uint32_t refs, void *arg) {
	(void)refs;
	(void)arg;
	struct wifi_object *o = payload;
	struct wifi_state *s = o->state;
	if (o->type == WIFI_AP) {
		struct wifi_object *parent = pointer_table_payload(o->parent);
		pak_wifi_unref_ap(s->net, parent->native, o->native);
	} else {
		pak_wifi_unref_adapter(s->net, o->native);
	}
	wasm_runtime_module_free(s->inst, o->guest);
	if (o->parent)
		pointer_table_release(o->parent);
	free(o);
}

/* Remove entries individually: AP destructors release acquired adapter nodes,
 * and pointer_table_destroy runs destructors while holding the table lock. */
void wasm_wifi_detach(wasm_module_inst_t inst) {
	struct wifi_state **link = &states;
	while (*link && (*link)->inst != inst)
		link = &(*link)->next;
	if (!*link)
		return;
	struct wifi_state *s = *link;
	*link = s->next;
	while (s->objects) {
		struct wifi_object *o = s->objects;
		s->objects = o->next;
		pointer_table_remove(s->table, o->guest, NULL);
	}
	pointer_table_destroy(s->table, NULL);
	if (s->owned)
		pak_net_unref_context(s->net);
	free(s);
}

static uint32_t add_object(struct wifi_state *s, uint32_t type, void *native, uint32_t parent_guest,
						   struct PtNode *parent) {
	if (!native) {
		if (parent)
			pointer_table_release(parent);
		return 0;
	}
	struct wifi_object *o = calloc(1, sizeof(*o));
	if (!o)
		goto fail;
	o->state = s;
	o->native = native;
	o->type = type;
	o->parent_guest = parent_guest;
	o->parent = parent;
	void *p = NULL;
	uint32_t size = type == WIFI_ADAPTER ? WASM_SIZEOF_PAK_WI_FI_ADAPTER : WASM_SIZEOF_PAK_WI_FI_AP;
	o->guest = wasm_runtime_module_malloc(s->inst, size, &p);
	if (!o->guest) {
		free(o);
		goto fail;
	}
	if (!pointer_table_add_with_key(s->table, o->guest, type, destroy_object, o)) {
		wasm_runtime_module_free(s->inst, o->guest);
		free(o);
		goto fail;
	}
	memset(p, 0, size);
	if (type == WIFI_ADAPTER) {
		struct PakWiFiAdapter *a = native;
		memcpy((uint8_t *)p + WASM_OFFSETOF_PAK_WI_FI_ADAPTER_NAME, a->name, sizeof(a->name));
		wasm_put_u32le((uint8_t *)p + WASM_OFFSETOF_PAK_WI_FI_ADAPTER_IS_ACTIVE, a->is_active);
	} else {
		struct PakWiFiAp *ap = native;
		memcpy((uint8_t *)p + WASM_OFFSETOF_PAK_WI_FI_AP_SSID, ap->ssid, sizeof(ap->ssid));
		memcpy((uint8_t *)p + WASM_OFFSETOF_PAK_WI_FI_AP_BSSID, ap->bssid, sizeof(ap->bssid));
		wasm_put_u32le((uint8_t *)p + WASM_OFFSETOF_PAK_WI_FI_AP_BAND, ap->band);
	}
	o->next = s->objects;
	s->objects = o;
	return o->guest;
fail:
	if (type == WIFI_ADAPTER)
		pak_wifi_unref_adapter(s->net, native);
	else {
		struct wifi_object *a = pointer_table_payload(parent);
		pak_wifi_unref_ap(s->net, a->native, native);
	}
	if (parent)
		pointer_table_release(parent);
	return 0;
}

static int remove_object(struct wifi_state *s, uint32_t key, uint32_t type, uint32_t parent) {
	struct PtNode *node = acquire(s, key, type);
	if (!node)
		return -1;
	struct wifi_object *o = pointer_table_payload(node);
	if (type == WIFI_AP && o->parent_guest != parent) {
		pointer_table_release(node);
		return -1;
	}
	struct wifi_object **link = &s->objects;
	while (*link && *link != o)
		link = &(*link)->next;
	if (*link)
		*link = o->next;
	pointer_table_user_unref(s->table, key, NULL);
	pointer_table_release(node);
	return 0;
}

static uint32_t wifi_context(wasm_exec_env_t env) {
	struct wifi_state *s = state_for(env);
	if (!s)
		return 0;
	if (!s->net) {
		s->net = pak_net_get_context();
		s->owned = s->net != NULL;
	}
	return s->net ? 1 : 0;
}

static void wifi_unref_context(wasm_exec_env_t env, uint32_t token) {
	(void)env;
	(void)token;
}

static int wifi_enabled(wasm_exec_env_t env, uint32_t token) {
	struct wifi_state *s = check(env, token);
	return s ? pak_wifi_is_enabled(s->net) : 0;
}

static int wifi_n_adapters(wasm_exec_env_t env, uint32_t token) {
	struct wifi_state *s = check(env, token);
	return s ? pak_wifi_get_n_adapters(s->net) : -1;
}

static uint32_t wifi_adapter(wasm_exec_env_t env, uint32_t token, int index) {
	struct wifi_state *s = check(env, token);
	return s ? add_object(s, WIFI_ADAPTER, pak_wifi_get_adapter(s->net, index), 0, NULL) : 0;
}

static int wifi_unref_adapter(wasm_exec_env_t env, uint32_t token, uint32_t key) {
	return remove_object(check(env, token), key, WIFI_ADAPTER, 0);
}

static int wifi_scan(wasm_exec_env_t env, uint32_t token, uint32_t key) {
	struct wifi_state *s = check(env, token);
	struct PtNode *n = acquire(s, key, WIFI_ADAPTER);
	if (!n)
		return -1;
	struct wifi_object *a = pointer_table_payload(n);
	int rc = pak_wifi_request_scan(s->net, a->native);
	pointer_table_release(n);
	return rc;
}

static int wifi_n_aps(wasm_exec_env_t env, uint32_t token, uint32_t key) {
	struct wifi_state *s = check(env, token);
	struct PtNode *n = acquire(s, key, WIFI_ADAPTER);
	if (!n)
		return -1;
	struct wifi_object *a = pointer_table_payload(n);
	int rc = pak_wifi_get_n_aps(s->net, a->native);
	pointer_table_release(n);
	return rc;
}

static uint32_t wifi_get_ap(wasm_exec_env_t env, uint32_t token, uint32_t key, int index, int connected) {
	struct wifi_state *s = check(env, token);
	struct PtNode *n = acquire(s, key, WIFI_ADAPTER);
	if (!n)
		return 0;
	struct wifi_object *a = pointer_table_payload(n);
	struct PakWiFiAp *ap =
		connected ? pak_wifi_get_connected_ap(s->net, a->native) : pak_wifi_get_ap(s->net, a->native, index);
	return add_object(s, WIFI_AP, ap, key, n); /* transfers acquired adapter reference */
}

static uint32_t wifi_ap(wasm_exec_env_t e, uint32_t t, uint32_t a, int index) {
	return wifi_get_ap(e, t, a, index, 0);
}

static uint32_t wifi_connected(wasm_exec_env_t e, uint32_t t, uint32_t a) {
	return wifi_get_ap(e, t, a, 0, 1);
}

static int wifi_unref_ap(wasm_exec_env_t env, uint32_t token, uint32_t adapter, uint32_t ap) {
	return remove_object(check(env, token), ap, WIFI_AP, adapter);
}

static int wifi_connect(wasm_exec_env_t env, uint32_t token, uint32_t adapter, uint32_t ap, uint32_t password,
						int blocking) {
	struct wifi_state *s = check(env, token);
	struct PtNode *an = acquire(s, adapter, WIFI_ADAPTER);
	struct PtNode *pn = acquire(s, ap, WIFI_AP);
	if (!an || !pn) {
		if (an)
			pointer_table_release(an);
		if (pn)
			pointer_table_release(pn);
		return -1;
	}
	struct wifi_object *a = pointer_table_payload(an), *p = pointer_table_payload(pn);
	int rc = -1;
	if (p->parent_guest == adapter) {
		const char *pw = NULL;
		if (!password || wasm_runtime_validate_app_str_addr(s->inst, password)) {
			if (password)
				pw = wasm_runtime_addr_app_to_native(s->inst, password);
			rc = pak_wifi_connect_to_ap(s->net, a->native, p->native, pw, blocking);
		}
	}
	pointer_table_release(pn);
	pointer_table_release(an);
	return rc;
}

static int wifi_bind(wasm_exec_env_t env, uint32_t token, uint32_t adapter, int fd) {
	struct wifi_state *s = check(env, token);
	struct PtNode *n = acquire(s, adapter, WIFI_ADAPTER);
	if (!n)
		return -1;
	struct wifi_object *a = pointer_table_payload(n);
	int host_fd = fd < 0 ? -1 : wasm_runtime_dup_wasi_socket(s->inst, (uint32_t)fd);
	int rc = host_fd < 0 ? -1 : pak_wifi_bind_socket_to_adapter(s->net, a->native, host_fd);
	if (host_fd >= 0)
		close(host_fd);
	pointer_table_release(n);
	return rc;
}

#define SYM(name, fn, sig) {name, (void *)(uintptr_t)fn, sig, NULL}

bool wasm_wifi_register(void) {
	static NativeSymbol symbols[] = {
		SYM("pak_net_get_context", wifi_context, "()i"),
		SYM("pak_net_unref_context", wifi_unref_context, "(i)"),
		SYM("pak_wifi_is_enabled", wifi_enabled, "(i)i"),
		SYM("pak_wifi_get_n_adapters", wifi_n_adapters, "(i)i"),
		SYM("pak_wifi_get_adapter", wifi_adapter, "(ii)i"),
		SYM("pak_wifi_unref_adapter", wifi_unref_adapter, "(ii)i"),
		SYM("pak_wifi_request_scan", wifi_scan, "(ii)i"),
		SYM("pak_wifi_get_n_aps", wifi_n_aps, "(ii)i"),
		SYM("pak_wifi_get_ap", wifi_ap, "(iii)i"),
		SYM("pak_wifi_get_connected_ap", wifi_connected, "(ii)i"),
		SYM("pak_wifi_unref_ap", wifi_unref_ap, "(iii)i"),
		SYM("pak_wifi_connect_to_ap", wifi_connect, "(iiiii)i"),
		SYM("pak_wifi_bind_socket_to_adapter", wifi_bind, "(iii)i"),
	};
	return wasm_runtime_register_natives("env", symbols, sizeof(symbols) / sizeof(symbols[0]));
}
