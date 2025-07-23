#ifndef MY_IMAGE_H
#define MY_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Define the Img struct type here
typedef struct {
    int width;
    int height;
    int pitch;
    const uint8_t *img_data;
} Img;

// Declare the image data and image instance
// extern const uint8_t mario_image_data[];
// extern const Img mario;

#ifdef __cplusplus
}
#endif

#endif // MY_IMAGE_H
