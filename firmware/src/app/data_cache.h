#ifndef APP_DATA_CACHE_H
#define APP_DATA_CACHE_H

#include <stdint.h>
#include <stdbool.h>

#define CACHE_MAX_SAMPLES 8000  /* 8000 s ~2.2 hours; 5 bytes each = 40 KB */

struct iv_sample {
	uint32_t uptime_ms;
	uint8_t  db;
};

void     data_cache_init(void);
void     data_cache_push(uint8_t db);
uint32_t data_cache_count(void);
bool     data_cache_get(uint32_t idx, struct iv_sample *out);
void     data_cache_clear(void);

#endif /* APP_DATA_CACHE_H */
