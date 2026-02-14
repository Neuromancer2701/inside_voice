#ifndef AUDIO_PDM_CAPTURE_H
#define AUDIO_PDM_CAPTURE_H

#include <stdint.h>
#include <stddef.h>

/* Audio capture parameters */
#define PDM_SAMPLE_RATE    16000
#define PDM_SAMPLE_BITS    16
#define PDM_CHANNELS       1
#define PDM_BLOCK_SAMPLES  1600  /* 100 ms at 16 kHz */
#define PDM_BLOCK_SIZE     (PDM_BLOCK_SAMPLES * sizeof(int16_t))
#define PDM_NUM_BLOCKS     4

/**
 * Initialize the PDM microphone via DMIC driver.
 * Configures 16 kHz mono 16-bit capture with memory slab buffering.
 *
 * @return 0 on success, negative errno on failure.
 */
int pdm_capture_init(void);

/**
 * Start continuous PDM capture.
 *
 * @return 0 on success, negative errno on failure.
 */
int pdm_capture_start(void);

/**
 * Read one block of audio samples (blocking).
 *
 * @param buf    Output: pointer to sample buffer (owned by slab, must be freed).
 * @param size   Output: number of bytes read.
 * @return 0 on success, negative errno on failure.
 */
int pdm_capture_read(void **buf, size_t *size);

/**
 * Release a buffer previously obtained from pdm_capture_read().
 *
 * @param buf  Buffer pointer to free back to the slab.
 */
void pdm_capture_buf_free(void *buf);

/**
 * Stop PDM capture.
 *
 * @return 0 on success, negative errno on failure.
 */
int pdm_capture_stop(void);

#endif /* AUDIO_PDM_CAPTURE_H */
