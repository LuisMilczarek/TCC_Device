#ifndef MAX6675_H
#define MAX6675_H

#include "driver/spi_master.h"
#include "esp_err.h"

/*enum SENSOR_SPI
{
    SPI2 = 2,
    SPI3
};
*/

typedef struct {
    spi_host_device_t spi;
    unsigned char DMA_channel;
    unsigned char CS;
    spi_device_handle_t spiHandle;    
    int16_t data;
    spi_transaction_t tm;
} MAX6675;

esp_err_t initMAX6675(MAX6675 *sensor);
esp_err_t getTemp(MAX6675 *sensor, float *temp);

#endif