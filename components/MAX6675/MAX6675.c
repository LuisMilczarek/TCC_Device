#include <stdio.h>
#include "MAX6675.h"
#include "esp_log.h"

static char *TAG = "MAX6675";

esp_err_t initMAX6675(MAX6675 *sensor)
{
    spi_bus_config_t buscfg = {};
    spi_device_interface_config_t devCfg = {};
    if(sensor->spi == SPI2_HOST)
    {
        buscfg.miso_io_num = 12;
        buscfg.sclk_io_num = 14;
        buscfg.mosi_io_num = 13;
    }
    else if(sensor->spi == SPI3_HOST)
    {
        buscfg.miso_io_num = 19;
        buscfg.sclk_io_num = 18;
        buscfg.mosi_io_num = 23;
    }
    else
    {
        ESP_LOGE(TAG, "Invalid spi host (SPI%d)!!!", sensor->spi);
        return ESP_FAIL;
    }
    
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = (4 * 8);
    
    devCfg.mode = 0;
    devCfg.clock_speed_hz = 2 * 1000 * 1000;
    devCfg.spics_io_num= sensor->CS;
    devCfg.queue_size = 3;
    
    if(sensor->spi == SPI2_HOST)
    {
        ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg,sensor->DMA_channel));
        ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST,&devCfg, &sensor->spiHandle));
    }
    else
    {
        ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg,sensor->DMA_channel));
        ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST,&devCfg, &sensor->spiHandle));
    }
    
    sensor->tm.tx_buffer = NULL;
    sensor->tm.rx_buffer = &sensor->data;
    sensor->tm.length = 16;
    sensor->tm.rxlength = 16;
    
    return ESP_OK;    
}

esp_err_t getTemp(MAX6675 *sensor, float *temp)
{
    spi_device_acquire_bus(sensor->spiHandle, portMAX_DELAY);
    spi_device_transmit(sensor->spiHandle, &sensor->tm);
    spi_device_release_bus(sensor->spiHandle);
    
    int16_t res = (int16_t) SPI_SWAP_DATA_RX(sensor->data, 16);
    
    if(res & (1 << 2))
    {
        ESP_LOGE(TAG, "Sensor is not connected");
        *temp = 0.0f;
        return ESP_FAIL;
    }
    else
    {
        res >>=3;
        *temp = res * 0.25;
        return ESP_OK;
    }
    
}
