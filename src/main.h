struct Module *pak_create_mod(void);

int pak_rt_test_module(struct Module *mod);

struct Module *pak_rt_mod_from_native(int (*get)(struct Module *mod));

int get_module_dummy(struct Module *mod);

int pak_rt_test_module(struct Module *mod);
