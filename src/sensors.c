#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include "sensors.h"

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

#define LIS3DH_NODE  DT_NODELABEL(lis3dh)
#define DS18B20_NODE DT_NODELABEL(ds18b20)

static const struct device *lis3dh_dev  = DEVICE_DT_GET(LIS3DH_NODE);
static const struct device *ds18b20_dev = DEVICE_DT_GET(DS18B20_NODE);

/* Step counter state — persists across calls */
static uint32_t step_count;
static bool     step_in_peak;

/* Thresholds in m/s² (gravity ≈ 9.81 m/s²) */
#define STEP_PEAK_THRESH  12.0f   /* rising edge — about 1.22 g */
#define STEP_VALLEY_THRESH 10.0f  /* falling edge — step complete */

int sensors_init(void)
{
	int err = 0;

	if (!device_is_ready(lis3dh_dev)) {
		LOG_ERR("LIS3DH not ready (check I²C wiring on D14/D15)");
		err = -ENODEV;
	} else {
		LOG_INF("LIS3DH ready");
	}

	if (!device_is_ready(ds18b20_dev)) {
		LOG_ERR("DS18B20 not ready (check 1-Wire wiring on D2, 4.7k pull-up)");
		err = err ? err : -ENODEV;
	} else {
		LOG_INF("DS18B20 ready");
	}

	return err;
}

int sensors_get_health(struct ble_health_t *out)
{
	/* Clear all fields; no HR/SpO2 sensor in this PoC */
	out->heart_rate     = 0;
	out->spo2           = 0;
	out->signal_quality = 0;
	out->flags          = 0;  /* bit0=HR, bit1=SpO2, bit2=Temp — all clear */

	if (!device_is_ready(ds18b20_dev)) {
		return -ENODEV;
	}

	/* DS18B20: fetch blocks ~750 ms for 12-bit resolution */
	int ret = sensor_sample_fetch(ds18b20_dev);
	if (ret) {
		LOG_WRN("DS18B20 fetch err: %d", ret);
		return ret;
	}

	struct sensor_value val;
	ret = sensor_channel_get(ds18b20_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (ret) {
		LOG_WRN("DS18B20 channel_get err: %d", ret);
		return ret;
	}

	/* sensor_value: val1 = integer °C, val2 = micro-fraction */
	int32_t temp_x10 = val.val1 * 10 + val.val2 / 100000;
	out->temperature = (int16_t)temp_x10;
	out->flags |= 0x04;  /* bit2: temperature valid */

	LOG_INF("Temp: %d.%d C", val.val1, val.val2 / 100000);
	return 0;
}

/* Classify activity from deviation from 1g (9.81 m/s²) */
static uint8_t classify_behavior(float mag)
{
	float dev = fabsf(mag - 9.81f);

	if (dev < 0.15f) return BEHAVIOR_SLEEPING;
	if (dev < 0.50f) return BEHAVIOR_RESTING;
	if (dev < 2.00f) return BEHAVIOR_WALKING;
	if (dev < 4.50f) return BEHAVIOR_RUNNING;
	return BEHAVIOR_PLAYING;
}

int sensors_get_behavior(struct ble_behavior_t *out)
{
	out->behavior   = BEHAVIOR_UNKNOWN;
	out->confidence = 0;
	out->steps      = step_count;

	if (!device_is_ready(lis3dh_dev)) {
		return -ENODEV;
	}

	int ret = sensor_sample_fetch(lis3dh_dev);
	if (ret) {
		LOG_WRN("LIS3DH fetch err: %d", ret);
		return ret;
	}

	struct sensor_value ax, ay, az;
	sensor_channel_get(lis3dh_dev, SENSOR_CHAN_ACCEL_X, &ax);
	sensor_channel_get(lis3dh_dev, SENSOR_CHAN_ACCEL_Y, &ay);
	sensor_channel_get(lis3dh_dev, SENSOR_CHAN_ACCEL_Z, &az);

	float x = (float)ax.val1 + (float)ax.val2 / 1000000.0f;
	float y = (float)ay.val1 + (float)ay.val2 / 1000000.0f;
	float z = (float)az.val1 + (float)az.val2 / 1000000.0f;
	float mag = sqrtf(x*x + y*y + z*z);

	/* Simple peak-valley step detection */
	if (!step_in_peak && mag > STEP_PEAK_THRESH) {
		step_in_peak = true;
		step_count++;
	} else if (step_in_peak && mag < STEP_VALLEY_THRESH) {
		step_in_peak = false;
	}

	out->behavior   = classify_behavior(mag);
	out->confidence = 75;
	out->steps      = step_count;

	LOG_DBG("accel mag: %d.%02d m/s2  steps: %u  behavior: %u",
		(int)mag, (int)(fabsf(mag - (int)mag) * 100),
		step_count, out->behavior);

	return 0;
}
