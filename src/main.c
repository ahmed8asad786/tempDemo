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
#include <math.h>
#include "my_image.h"

#define DHT_NODE DT_PATH(dht11)

#define DISPLAY_WIDTH      256
#define DISPLAY_HEIGHT     120
#define DISPLAY_PITCH      DISPLAY_WIDTH
#define DISPLAY_BYTE_PITCH ((DISPLAY_WIDTH + 7) / 8)
#define DISPLAY_BUF_SIZE   (DISPLAY_BYTE_PITCH * DISPLAY_HEIGHT)

extern const Img raindrop;
extern const Img flame;

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


//image drawn method, used for raindrop and temperature icons
void draw_my_image(int x_offset, int y_offset, const Img  *image) {
   int width = image->width;
   int height = image->height;
   int bytes_per_row = image->pitch;  // (100 + 7) / 8


   for (int y = 0; y < height; y++) {
       for (int byte_index = 0; byte_index < bytes_per_row; byte_index++) {
           uint8_t byte = image->img_data[y * bytes_per_row + byte_index];
           for (int bit = 0; bit < 8; bit++) {
               int x = byte_index * 8 + bit;
               if (x >= width) break;  


               if (byte & (0x80 >> bit)) {
                   set_pixel(x + x_offset, y + y_offset);
               }
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

int last_temp = INT32_MIN;  
int last_humid= INT32_MIN;
    int count= 0;
while (1) {
    struct sensor_value temp;
    struct sensor_value humidity;
    if (sensor_sample_fetch(dht_dev) == 0 &&
        sensor_channel_get(dht_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) == 0 &&
        sensor_channel_get(dht_dev, SENSOR_CHAN_HUMIDITY, &humidity)==0){
            
         float temp_c = temp.val1 + temp.val2 / 1000000.0f;
        float rh = humidity.val1 + humidity.val2 / 1000000.0f;
        float temp_f = temp_c * 9.0f / 5.0f + 32.0f;

        // Calculate Heat Index in Fahrenheit using the exact formula
        float HI_f = -42.379f
                   + 2.04901523f * temp_f
                   + 10.14333127f * rh
                   - 0.22475541f * temp_f * rh
                   - 0.00683783f * temp_f * temp_f
                   - 0.05481717f * rh * rh
                   + 0.00122874f * temp_f * temp_f * rh
                   + 0.00085282f * temp_f * rh * rh
                   - 0.00000199f * temp_f * temp_f * rh * rh;

        // Convert result back to Celsius
        int heat_index_celsius = (int)((HI_f - 32.0f) * 5.0f / 9.0f);

        

              if (temp.val1 != last_temp || humidity.val1 != last_humid) {
            last_temp = temp.val1;
            last_humid = humidity.val1;

            memset(raw_buf, 0xFF, sizeof(raw_buf));  // Clear to white before drawing

            char temp_str[50];
            char humidity_str[50];
            char heat_index_str[50];
            snprintk(temp_str, sizeof(temp_str), "Temperature %d.%02d,C", temp.val1, temp.val2 / 10000);
            snprintk(humidity_str, sizeof(humidity_str), "Humidity %d.%02d%%", humidity.val1, humidity.val2 / 10000);
            if(temp.val1>=26){
            snprintk(heat_index_str, sizeof(heat_index_str), "Heat Index %d.%02d,C", heat_index_celsius);
    
            }
            else{
            snprintk(heat_index_str, sizeof(heat_index_str), "Heat Index: %d.%02d,C", temp.val1);
            }
            
            printk("Temp: %d.%06d C, Humidity: %d.%06d%%\n", temp.val1, temp.val2, humidity.val1, humidity.val2);
            printk("Heat Index: %d\n", heat_index_celsius);
            if (count%2==0){
            draw_string_8x10(temp_str, 9, 56);
            draw_string_8x10(humidity_str, 9, 20);
            draw_string_8x10(heat_index_str, 9, 91);
             if(humidity.val1>=50){
                draw_my_image(146, 13, &raindrop);
             }
            
             if (heat_index_celsius>=26){
                draw_my_image(192, 56, &flame);
             }
            }
            else{
                draw_string_8x10(temp_str, 10, 57);
                draw_string_8x10(humidity_str, 10, 21);
                draw_string_8x10(heat_index_str, 10, 92);
                     if(humidity.val1>=50){
                draw_my_image(147, 14, &raindrop);
             }
             if (heat_index_celsius>=26){
                draw_my_image(193, 57, &flame);
             }
            }
            pack_buffer();
            display_write(display_dev, 0, 0, &desc, buf);
            memset(raw_buf, 0xFF, sizeof(raw_buf)); 
            count++;

        } else {
            printk("Temp and humidity unchanged (%d°C and %d%%), skipping refresh\n", temp.val1, humidity.val1);
        }
    }

     else {
        printk("Failed to read from DHT11\n");
    }

    k_sleep(K_SECONDS(1));
}
}