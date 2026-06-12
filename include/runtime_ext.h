#include <stdint.h>

typedef uint8_t *get_additional_bytes(void *arg, uint8_t *buffer, unsigned int old_len, unsigned int last_len);

struct ExifParser {
	uint8_t *buf;
	unsigned int length;
	unsigned int exif_start;
	void *arg;
	get_additional_bytes *get_more;
	int subifd;
	unsigned int thumb_of;
	unsigned int thumb_size;
};

int exif_start_raw(struct ExifParser *c, uint8_t *buf, unsigned int length, get_additional_bytes *get_more, void *arg);

/// Execute JS file and get the exported module handle
int setup_quickjs_module(struct Module *mod, char *file_contents, unsigned int length); 

int setup_wasm_module(struct Module **mod, const char *filename);
