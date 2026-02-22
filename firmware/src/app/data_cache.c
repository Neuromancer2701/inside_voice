#include <zephyr/kernel.h>
#include "data_cache.h"

static struct iv_sample cache[CACHE_MAX_SAMPLES];
static uint32_t head;
static uint32_t tail;
static uint32_t count;

K_MUTEX_DEFINE(cache_mutex);

void data_cache_init(void)
{
	k_mutex_lock(&cache_mutex, K_FOREVER);
	head = 0;
	tail = 0;
	count = 0;
	k_mutex_unlock(&cache_mutex);
}

void data_cache_push(uint8_t db)
{
	k_mutex_lock(&cache_mutex, K_FOREVER);

	if (count == CACHE_MAX_SAMPLES) {
		/* Overwrite oldest */
		tail = (tail + 1) % CACHE_MAX_SAMPLES;
	} else {
		count++;
	}

	cache[head].uptime_ms = (uint32_t)k_uptime_get();
	cache[head].db = db;
	head = (head + 1) % CACHE_MAX_SAMPLES;

	k_mutex_unlock(&cache_mutex);
}

uint32_t data_cache_count(void)
{
	k_mutex_lock(&cache_mutex, K_FOREVER);
	uint32_t n = count;
	k_mutex_unlock(&cache_mutex);
	return n;
}

bool data_cache_get(uint32_t idx, struct iv_sample *out)
{
	k_mutex_lock(&cache_mutex, K_FOREVER);

	if (idx >= count) {
		k_mutex_unlock(&cache_mutex);
		return false;
	}

	uint32_t actual_idx = (tail + idx) % CACHE_MAX_SAMPLES;
	*out = cache[actual_idx];

	k_mutex_unlock(&cache_mutex);
	return true;
}

void data_cache_clear(void)
{
	k_mutex_lock(&cache_mutex, K_FOREVER);
	head = 0;
	tail = 0;
	count = 0;
	k_mutex_unlock(&cache_mutex);
}
