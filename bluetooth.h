struct PakBt;

struct PakBt *pak_bt_get_context(void);

struct PakBtAdapter {
	char address[64];
	char name[64];
	int powered;
	int class;
	void *priv;
};

struct PakBtAdapterList {
	int length;
	struct PakBtAdapter list[];
};

int pak_bt_get_adapters(struct PakBt *ctx, struct PakBtAdapterList **list_arg);
