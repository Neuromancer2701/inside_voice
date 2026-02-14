#include "sound_level.h"

#include <zephyr/sys/util.h>

/*
 * Integer-only sound level computation.
 *
 * RMS is computed with 64-bit accumulation to avoid overflow on buffers
 * up to ~65k samples of 16-bit audio.
 *
 * dB conversion uses a lookup-table-assisted integer log2 approximation.
 * Reference: 0 dBFS = RMS of 32767.  We add a fixed offset (90) to shift
 * into a pseudo-SPL range that's more intuitive for the user.
 */

uint16_t sound_level_rms(const int16_t *samples, size_t count)
{
	if (count == 0) {
		return 0;
	}

	uint64_t sum_sq = 0;

	for (size_t i = 0; i < count; i++) {
		int32_t s = samples[i];
		sum_sq += (uint64_t)(s * s);
	}

	uint64_t mean_sq = sum_sq / count;

	/* Integer square root via Newton's method */
	if (mean_sq == 0) {
		return 0;
	}

	uint32_t x = (uint32_t)mean_sq;
	uint32_t y = x;

	while (1) {
		uint32_t next = (y + x / y) / 2;
		if (next >= y) {
			break;
		}
		y = next;
	}

	return (uint16_t)MIN(y, UINT16_MAX);
}

/*
 * Approximate 20*log10(rms/32767) using integer math.
 *
 * log10(x) = log2(x) / log2(10) ≈ log2(x) * 0.30103
 * 20 * log10(x) ≈ 6.0206 * log2(x)
 *
 * We compute log2 via bit position + fractional lookup, then multiply by
 * ~6.02 using fixed-point (6 + 1/51 ≈ 6.0196).
 *
 * Result is dBFS (negative), offset by +90 to give a pseudo-SPL reading.
 */

/* 4-bit fractional log2 lookup (0..15 → 0..15 in 1/16ths of a log2 step) */
static const uint8_t log2_frac_lut[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static uint16_t ilog2_fixed4(uint16_t val)
{
	if (val == 0) {
		return 0;
	}

	/* Find integer part: position of highest set bit */
	uint8_t int_part = 0;
	uint16_t tmp = val;

	while (tmp >>= 1) {
		int_part++;
	}

	/* Fractional part: next 4 bits after the leading 1 */
	uint8_t frac_bits;

	if (int_part >= 4) {
		frac_bits = (val >> (int_part - 4)) & 0x0F;
	} else {
		frac_bits = (val << (4 - int_part)) & 0x0F;
	}

	/* Result in 4.4 fixed point */
	return ((uint16_t)int_part << 4) | log2_frac_lut[frac_bits];
}

uint8_t sound_level_rms_to_db(uint16_t rms)
{
	if (rms == 0) {
		return 0;
	}

	/*
	 * dBFS = 20 * log10(rms / 32767)
	 *      = 6.02 * (log2(rms) - log2(32767))
	 *      = 6.02 * (log2(rms) - 15)
	 *
	 * We want pseudo-SPL = dBFS + 90
	 */
	uint16_t log2_val = ilog2_fixed4(rms);       /* 4.4 fixed point */
	uint16_t log2_ref = ilog2_fixed4(32767);      /* ~15.0 in 4.4 = 240 */

	/* dBFS in 4.4 fixed point, will be negative for rms < 32767 */
	int16_t dbfs_fixed = (int16_t)log2_val - (int16_t)log2_ref;

	/* Multiply by 6 (approximate 6.02) and shift back from 4.4 → integer */
	int16_t db = (dbfs_fixed * 6) >> 4;

	/* Offset to pseudo-SPL range */
	db += 90;

	return (uint8_t)CLAMP(db, 0, 90);
}
