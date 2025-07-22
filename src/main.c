#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include "text.h"
#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

#define DHT_NODE DT_PATH(dht11)

#define DISPLAY_WIDTH      256
#define DISPLAY_HEIGHT     120
#define DISPLAY_PITCH      DISPLAY_WIDTH
#define DISPLAY_BYTE_PITCH ((DISPLAY_WIDTH + 7) / 8)
#define DISPLAY_BUF_SIZE   (DISPLAY_BYTE_PITCH * DISPLAY_HEIGHT)

LOG_MODULE_REGISTER(main);
#define PIXEL_ON 0
#define PIXEL_OFF 1

static uint8_t raw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT];  // 1 byte per pixel
static uint8_t buf[DISPLAY_BUF_SIZE];



extern void set_pixel(int x, int y) {
   if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
       return;
   }
   raw_buf[y * DISPLAY_WIDTH + x] = PIXEL_ON;
}

static void color_white(int x, int y){
      if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
       return;
   }
   raw_buf[y * DISPLAY_WIDTH + x] = PIXEL_OFF;
   
}
static void pack_buffer(void) {
   memset(buf, 0xFF, sizeof(buf));  // Set all to white
   for (int y = 0; y < DISPLAY_HEIGHT; y++) {
       for (int x = 0; x < DISPLAY_WIDTH; x++) {
           if (raw_buf[y * DISPLAY_WIDTH + x] == PIXEL_ON) {
               int byte_index = x + (y / 8) * DISPLAY_WIDTH;
               int bit_pos = 7 - (y % 8);
               buf[byte_index] &= ~(1 << bit_pos);
               // LOG_INF("PACK: (x=%d y=%d) -> Byte %d, bit %d", x, y, byte_index, bit_pos);
           }
       }
   }
}





int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
   const struct device *reset_dev   = device_get_binding("GPIO_1");
   const struct device *spi_dev     = DEVICE_DT_GET(DT_NODELABEL(spi0));
   const struct device *gpio_dev    = DEVICE_DT_GET(DT_NODELABEL(gpio1));


   if (reset_dev) {
       gpio_pin_configure(reset_dev, 3, GPIO_OUTPUT);
       gpio_pin_set(reset_dev, 3, 0);
       k_sleep(K_MSEC(10));
       gpio_pin_set(reset_dev, 3, 1);
       k_sleep(K_MSEC(10));
   }    


   LOG_INF("SPI ready? %d", device_is_ready(spi_dev));
   LOG_INF("GPIO1 ready? %d", device_is_ready(gpio_dev));
   LOG_INF("Chosen display node: %s", DT_NODE_FULL_NAME(DT_CHOSEN(zephyr_display)));


   LOG_INF("Delaying for display power up");
   k_msleep(100);


   if (!device_is_ready(display_dev)) {
       LOG_ERR("Display device not ready");
       return -1;
   }


   LOG_INF("DEVICE IS READY!!!");
   LOG_INF("Display device name: %s", display_dev->name);


   display_blanking_off(display_dev);
   memset(raw_buf, 1, sizeof(raw_buf));  // White pixels


   struct display_buffer_descriptor desc = {
       .width = DISPLAY_WIDTH,
       .height = DISPLAY_HEIGHT,
       .pitch = DISPLAY_PITCH,
       .buf_size = DISPLAY_BUF_SIZE,
   };


    const struct device *dht_dev = DEVICE_DT_GET(DHT_NODE);

    if (!device_is_ready(dht_dev)) {
        printk("DHT11 not ready\n");
        return 1;
    }

    printk("DHT11 sensor ready\n");

 while (1) {
    struct sensor_value temp;

    if (sensor_sample_fetch(dht_dev) == 0 &&
        sensor_channel_get(dht_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) == 0) {

        char str[50];

        // Start with prefix
        snprintk(str, sizeof(str), "Temperature is: %d", temp.val1);

        // Append suffix
        strncat(str, " Celsius", sizeof(str) - strlen(str) - 1);

        printk("Temp: %d.%06d C\n", temp.val1, temp.val2);
        printk("%s\n", str);
        draw_string_7x9(str, 10, 50);

        pack_buffer();
        display_write(display_dev, 0, 0, &desc, buf);
        memset(raw_buf, 0xFF, sizeof(raw_buf));  // Clear after draw
    } else {
        printk("Failed to read from DHT11\n");
    }

    k_sleep(K_SECONDS(3));
}


}
