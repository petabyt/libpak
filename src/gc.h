enum ContainerType {
	BT_DEVICE_PRIV,
	BT_ADAPTER_PRIV,
	GATT_SVC_PRIV,
	GATT_CHR_PRIV,
	GATT_DESC_PRIV,
	BT_SOCKET,
	WIFI_ADAPTER_PRIV,
	WIFI_AP_PRIV,
};

struct Container {
	void *value;
	void (*free)(void *value, void *arg);
	void *free_arg;
	enum ContainerType type;
};

struct GcContext {
	struct Container *list;
	unsigned int list_length;
};

void pak_setup_gc(struct GcContext *ctx);
void pak_gc_add(struct GcContext *ctx, enum ContainerType type, void *ptr);
void pak_gc_add_ex(struct GcContext *ctx, enum ContainerType type, void *ptr, void (*free)(void *value, void *arg), void *free_arg);
void pak_gc_remove(struct GcContext *ctx, void *ptr);
void pak_gc_close(struct GcContext *ctx);
