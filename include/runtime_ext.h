// Extras, not runtime stuff
#include <stdint.h>

/// Initialize QuickJS wrapper around PakModule
int setup_quickjs_module(struct PakModule *mod, char *file_contents, unsigned int length);

// Initialize WAMR wrapper around PakModule
int setup_wasm_module(struct PakModule *mod, char *file_contents, unsigned int file_length);
