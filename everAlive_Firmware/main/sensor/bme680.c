#include "bme680.h"
#include "common.h"
#include "driver/i2c_master.h"
#include "bme68x.h"
#include "bme68x_defs.h"
#include <string.h>


static i2c_master_bus_handle_t  i2c_bus    = NULL;
static i2c_master_dev_handle_t  i2c_dev    = NULL;
static struct bme68x_dev        bme_dev    = {0};


static BME68X_INTF_RET_TYPE bme68x_i2c_read(uint8_t reg_addr,
                                              uint8_t *reg_data,
                                              uint32_t len,
                                              void *intf_ptr)
{
    (void)intf_ptr;
    esp_err_t ret;

    /* Write register address */
    ret = i2c_master_transmit(i2c_dev, &reg_addr, 1, 100);
    if (ret != ESP_OK) return BME68X_E_COM_FAIL;

    /* Read data */
    ret = i2c_master_receive(i2c_dev, reg_data, len, 100);
    return (ret == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static BME68X_INTF_RET_TYPE bme68x_i2c_write(uint8_t reg_addr,
                                               const uint8_t *reg_data,
                                               uint32_t len,
                                               void *intf_ptr)
{
    (void)intf_ptr;

    /* Buffer: [reg_addr, data...] */
    uint8_t buf[len + 1];
    buf[0] = reg_addr;
    memcpy(&buf[1], reg_data, len);

    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, len + 1, 100);
    return (ret == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static void bme68x_delay_us(uint32_t period, void *intf_ptr) {
    (void)intf_ptr;
    esp_rom_delay_us(period);
}

esp_err_t bme680_sensor_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = BME680_I2C_PORT,
        .sda_io_num          = BME680_SDA_PIN,
        .scl_io_num          = BME680_SCL_PIN,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BME680_I2C_ADDR,
        .scl_speed_hz    = BME680_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev));

    bme_dev.intf     = BME68X_I2C_INTF;
    bme_dev.intf_ptr = NULL;
    bme_dev.read     = bme68x_i2c_read;
    bme_dev.write    = bme68x_i2c_write;
    bme_dev.delay_us = bme68x_delay_us;
    bme_dev.amb_temp = 25;   /*Env temp compesation*/

    int8_t rslt = bme68x_init(&bme_dev);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "bme68x_init failed: %d", rslt);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BME680 init (chip_id=0x%02X)", bme_dev.chip_id);
    return ESP_OK;
}

esp_err_t bme680_sensor_read(sensor_payload_t *payload) {
    if (!payload) return ESP_ERR_INVALID_ARG;

    /* Min oversampling without heater to speed up measurement.*/
    struct bme68x_conf conf = {
        .filter  = BME68X_FILTER_OFF,
        .odr     = BME68X_ODR_NONE,
        .os_hum  = BME68X_OS_1X,
        .os_pres = BME68X_OS_1X,
        .os_temp = BME68X_OS_1X,
    };
    int8_t rslt = bme68x_set_conf(&conf, &bme_dev);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "bme68x_set_conf failed: %d", rslt);
        return ESP_FAIL;
    }

    /* Gas heater OFF */
    struct bme68x_heatr_conf heatr = {
        .enable   = BME68X_DISABLE,
        .heatr_temp = 0,
        .heatr_dur  = 0,
    };
    rslt = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr, &bme_dev);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "bme68x_set_heatr_conf failed: %d", rslt);
        return ESP_FAIL;
    }

    /* Run force mode, to improve measurement speed.*/
    rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, &bme_dev);
    if (rslt != BME68X_OK) {
        ESP_LOGE(TAG, "bme68x_set_op_mode failed: %d", rslt);
        return ESP_FAIL;
    }

    /* Wait minimun calculated time. */
    uint32_t delay_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme_dev);
    bme_dev.delay_us(delay_us + 500, NULL);

    /* Read result */
    struct bme68x_data data = {0};
    uint8_t n_data = 0;
    rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_data, &bme_dev);
    if (rslt != BME68X_OK || n_data == 0) {
        ESP_LOGE(TAG, "bme68x_get_data failed: rslt=%d n=%d", rslt, n_data);
        return ESP_FAIL;
    }

    payload->temperature = (int16_t)(data.temperature * 100.0f);
    payload->humidity    = (uint16_t)(data.humidity    * 100.0f);
    payload->pressure    = (uint16_t)(data.pressure / 100.0f * 10.0f);

    ESP_LOGI(TAG, "BME680: T=%.2f°C  H=%.2f%%  P=%.1fhPa",
             data.temperature, data.humidity, data.pressure / 100.0f);
    return ESP_OK;
}

void bme680_sensor_deinit(void) {

    bme68x_set_op_mode(BME68X_SLEEP_MODE, &bme_dev);
    
    if (i2c_dev) {
        i2c_master_bus_rm_device(i2c_dev);
        i2c_dev = NULL;
    }
    if (i2c_bus) {
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
    }
}