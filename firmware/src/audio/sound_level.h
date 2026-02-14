#ifndef AUDIO_SOUND_LEVEL_H
#define AUDIO_SOUND_LEVEL_H

#include <stdint.h>
#include <stddef.h>

/**
 * Compute integer RMS of 16-bit PCM samples.
 *
 * @param samples  Pointer to signed 16-bit PCM buffer.
 * @param count    Number of samples.
 * @return RMS amplitude (0–32767).
 */
uint16_t sound_level_rms(const int16_t *samples, size_t count);

/**
 * Convert RMS amplitude to approximate dB SPL.
 *
 * Uses an integer log2 approximation — no FPU required.
 * The result is referenced to full-scale (0 dB = 32767).
 * Practical range is roughly 30–90 dB after offset.
 *
 * @param rms  RMS amplitude from sound_level_rms().
 * @return Approximate dB value (0–90 range, clamped).
 */
uint8_t sound_level_rms_to_db(uint16_t rms);

#endif /* AUDIO_SOUND_LEVEL_H */
