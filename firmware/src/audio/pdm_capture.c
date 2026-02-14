#include "pdm_capture.h"

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pdm_capture, LOG_LEVEL_INF);

K_MEM_SLAB_DEFINE_STATIC(pdm_slab, PDM_BLOCK_SIZE, PDM_NUM_BLOCKS, 4);

static const struct device *dmic_dev;

int pdm_capture_init(void)
{
	dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm0));
	if (!device_is_ready(dmic_dev)) {
		LOG_ERR("PDM device not ready");
		return -ENODEV;
	}

	struct pcm_stream_cfg stream_cfg = {
		.pcm_rate = PDM_SAMPLE_RATE,
		.pcm_width = PDM_SAMPLE_BITS,
		.block_size = PDM_BLOCK_SIZE,
		.mem_slab = &pdm_slab,
	};

	struct dmic_cfg cfg = {
		.io = {
			.min_pdm_clk_freq = 1024000,
			.max_pdm_clk_freq = 3072000,
			.min_pdm_clk_dc = 40,
			.max_pdm_clk_dc = 60,
			.pdm_clk_pol = 0,
			.pdm_data_pol = 0,
		},
		.streams = &stream_cfg,
		.channel = {
			.req_num_streams = 1,
			.req_num_chan = PDM_CHANNELS,
			.req_chan_map_lo =
				dmic_build_channel_map(0, 0, PDM_CHAN_LEFT),
		},
	};

	int err = dmic_configure(dmic_dev, &cfg);

	if (err) {
		LOG_ERR("dmic_configure failed: %d", err);
		return err;
	}

	LOG_INF("PDM mic initialized: %u Hz, %u-bit, mono",
		PDM_SAMPLE_RATE, PDM_SAMPLE_BITS);
	return 0;
}

int pdm_capture_start(void)
{
	int err = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);

	if (err) {
		LOG_ERR("dmic_trigger START failed: %d", err);
	}
	return err;
}

int pdm_capture_read(void **buf, size_t *size)
{
	uint32_t bytes = 0;
	int err = dmic_read(dmic_dev, 0, buf, &bytes, K_FOREVER);

	if (err) {
		LOG_ERR("dmic_read failed: %d", err);
		return err;
	}

	*size = bytes;
	return 0;
}

void pdm_capture_buf_free(void *buf)
{
	k_mem_slab_free(&pdm_slab, buf);
}

int pdm_capture_stop(void)
{
	int err = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);

	if (err) {
		LOG_ERR("dmic_trigger STOP failed: %d", err);
	}
	return err;
}
