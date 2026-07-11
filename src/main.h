struct PakModule *pak_create_mod(void);

int pak_rt_test_module(struct PakModule *mod);

struct PakModule *pak_rt_mod_from_native(int (*get)(struct PakModule *mod));

int get_module_dummy(struct PakModule *mod);

int pak_rt_test_module(struct PakModule *mod);
