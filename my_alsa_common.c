#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_alsa_common.h"
/*
https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm_8c-example.html
*/

#if SUPPORT_TINYALSA == 1

int xrun_recovery(snd_pcm_t *handle, int err)
{
	printf("stream recovery\n");
	if (err == -EPIPE) {    /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);       /* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

int pcm_format_to_alsa(enum pcm_format format)
{
	switch (format) {

	case PCM_FORMAT_S8:
			return SND_PCM_FORMAT_S8;

	default:
	case PCM_FORMAT_S16_LE:
		return SND_PCM_FORMAT_S16_LE;
	case PCM_FORMAT_S16_BE:
		return SND_PCM_FORMAT_S16_BE;

	case PCM_FORMAT_S24_LE:
		return SND_PCM_FORMAT_S24_LE;
	case PCM_FORMAT_S24_BE:
		return SND_PCM_FORMAT_S24_BE;

	case PCM_FORMAT_S24_3LE:
		return SND_PCM_FORMAT_S24_3LE;
	case PCM_FORMAT_S24_3BE:
		return SND_PCM_FORMAT_S24_3BE;

	case PCM_FORMAT_S32_LE:
		return SND_PCM_FORMAT_S32_LE;
	case PCM_FORMAT_S32_BE:
		return SND_PCM_FORMAT_S32_BE;
	};
}

int SetParametersByTinyAlsaConfigs(snd_pcm_t *pHandle, snd_pcm_hw_params_t *hwparams, struct pcm_config *pConfigsIn)
{
	int vRet = 0;
	int vDirection = 0;
	snd_pcm_format_t    vFormat;
	//snd_pcm_uframes_t vFrames;
	snd_pcm_sw_params_t *swparams;

	// tinyalsa configuration
	struct pcm_config *pConfigs = (struct pcm_config *)pConfigsIn;

	if (!pConfigs)
		return -1;

	/* Fill it in with default vValues. */
	snd_pcm_hw_params_any(pHandle, hwparams);

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(pHandle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);

	vFormat = pcm_format_to_alsa(pConfigs->format);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(pHandle, hwparams, vFormat); // SND_PCM_FORMAT_S16_LE

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(pHandle, hwparams, pConfigs->channels); // 2

	/* 44100 bits/second sampling rate (CD quality) */
	snd_pcm_hw_params_set_rate_near(pHandle, hwparams, &pConfigs->rate, &vDirection); // 44100

	/* Set period size to 32 frames. */
	//vFrames = 32;
	//snd_pcm_hw_params_set_period_size_near(pHandle, hwparams, &vFrames, &vDirection);
	snd_pcm_hw_params_set_period_size(pHandle, hwparams, pConfigs->period_size, vDirection);	// 1024
	snd_pcm_hw_params_set_periods(pHandle, hwparams, pConfigs->period_count, vDirection);  // 4

	/* Write the parameters to the driver */
	vRet = snd_pcm_hw_params(pHandle, hwparams);
	if (vRet < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(vRet));
		exit(1);
	}

	snd_pcm_sw_params_alloca(&swparams);

	/* get the current swparams */
	vRet = snd_pcm_sw_params_current(pHandle, swparams);
	if (vRet < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(vRet));
		return vRet;
	}

	int period_event = 0;
	int buffer_size = pConfigs->period_size;
	int period_size = pConfigs->period_size;
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	vRet = snd_pcm_sw_params_set_start_threshold(pHandle, swparams, (buffer_size / period_size) * period_size);
	if (vRet < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(vRet));
		return vRet;
	}

	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	vRet = snd_pcm_sw_params_set_avail_min(pHandle, swparams, period_event ? buffer_size : period_size);
	if (vRet < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(vRet));
		return vRet;
	}

	/* enable period events when requested */
	if (period_event) {
		vRet = snd_pcm_sw_params_set_period_event(pHandle, swparams, 1);
		if (vRet < 0) {
			printf("Unable to set period event: %s\n", snd_strerror(vRet));
			return vRet;
		}
	}

	/* Write the parameters to the driver */
	vRet = snd_pcm_sw_params(pHandle, swparams);
	if (vRet < 0) {
		fprintf(stderr, "unable to set sw parameters: %s\n", snd_strerror(vRet));
		exit(1);
	}

	return 0;
}


#else 

int SetParametersByAlsaConfigs(snd_pcm_t *pHandle, snd_pcm_hw_params_t *pParams)
{
	int vRet, vDirection;
	unsigned int vVal;
	snd_pcm_uframes_t vFrames;

	/* Fill it in with default vValues. */
	snd_pcm_hw_params_any(pHandle, pParams);

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(pHandle, pParams, SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(pHandle, pParams, SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(pHandle, pParams, 2);

	/* 44100 bits/second sampling rate (CD quality) */
	vVal = 44100;
	snd_pcm_hw_params_set_rate_near(pHandle, pParams, &vVal, &vDirection);

	/* Set period size to 32 frames. */
	vFrames = 32;
	snd_pcm_hw_params_set_period_size_near(pHandle, pParams, &vFrames, &vDirection);

	/* Write the parameters to the driver */
	vRet = snd_pcm_hw_params(pHandle, pParams);
	if (vRet < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(vRet));
		exit(1);
	}
}

#endif

void ShowAlsaParameters(snd_pcm_t *pHandle, snd_pcm_hw_params_t *pParams)
{
	snd_pcm_uframes_t vxFrames;
	unsigned int vVal, vVal2;
	int vDirection;

	/* Display information about the PCM interface */

	printf("PCM pHandle name = '%s'\n", snd_pcm_name(pHandle));
	printf("PCM state = %s\n", snd_pcm_state_name(snd_pcm_state(pHandle)));

	snd_pcm_hw_params_get_access(pParams, (snd_pcm_access_t *) &vVal);
	printf("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)vVal));

	snd_pcm_hw_params_get_format(pParams, (snd_pcm_format_t *)&vVal);
	printf("format = '%s' (%s)\n",
	       snd_pcm_format_name((snd_pcm_format_t)vVal),
	       snd_pcm_format_description((snd_pcm_format_t)vVal));

	snd_pcm_hw_params_get_subformat(pParams, (snd_pcm_subformat_t *)&vVal);
	printf("subformat = '%s' (%s)\n",
	       snd_pcm_subformat_name((snd_pcm_subformat_t)vVal),
	       snd_pcm_subformat_description((snd_pcm_subformat_t)vVal));

	snd_pcm_hw_params_get_channels(pParams, &vVal);
	printf("channels = %d\n", vVal);

	snd_pcm_hw_params_get_rate(pParams, &vVal, &vDirection);
	printf("rate = %d bps\n", vVal);

	snd_pcm_hw_params_get_period_time(pParams, &vVal, &vDirection);
	printf("period time = %d us\n", vVal);

	snd_pcm_hw_params_get_period_size(pParams, &vxFrames, &vDirection);
	printf("period size = %d vxFrames\n", (int)vxFrames);

	snd_pcm_hw_params_get_buffer_time(pParams, &vVal, &vDirection);
	printf("buffer time = %d us\n", vVal);

	snd_pcm_hw_params_get_buffer_size(pParams, (snd_pcm_uframes_t *) &vVal);
	printf("buffer size = %d vxFrames\n", vVal);

	snd_pcm_hw_params_get_periods(pParams, &vVal, &vDirection);
	printf("periods per buffer = %d vxFrames\n", vVal);

	snd_pcm_hw_params_get_rate_numden(pParams, &vVal, &vVal2);
	printf("exact rate = %d/%d bps\n", vVal, vVal2);

	vVal = snd_pcm_hw_params_get_sbits(pParams);
	printf("significant bits = %d\n", vVal);

	//snd_pcm_hw_params_get_tick_time(pParams, &vVal, &vDirection);
	//printf("tick time = %d us\n", vVal);

	vVal = snd_pcm_hw_params_is_batch(pParams);
	printf("is batch = %d\n", vVal);

	vVal = snd_pcm_hw_params_is_block_transfer(pParams);
	printf("is block transfer = %d\n", vVal);

	vVal = snd_pcm_hw_params_is_double(pParams);
	printf("is double = %d\n", vVal);

	vVal = snd_pcm_hw_params_is_half_duplex(pParams);
	printf("is half duplex = %d\n", vVal);

	vVal = snd_pcm_hw_params_is_joint_duplex(pParams);
	printf("is joint duplex = %d\n", vVal);

	vVal = snd_pcm_hw_params_can_overrange(pParams);
	printf("can overrange = %d\n", vVal);

	vVal = snd_pcm_hw_params_can_mmap_sample_resolution(pParams);
	printf("can mmap = %d\n", vVal);

	vVal = snd_pcm_hw_params_can_pause(pParams);
	printf("can pause = %d\n", vVal);

	vVal = snd_pcm_hw_params_can_resume(pParams);
	printf("can resume = %d\n", vVal);

	vVal = snd_pcm_hw_params_can_sync_start(pParams);
	printf("can sync start = %d\n", vVal);
}