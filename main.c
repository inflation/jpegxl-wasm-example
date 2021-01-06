#include "emscripten.h"

#include <jxl/decode.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

EMSCRIPTEN_KEEPALIVE
int version() { return JxlDecoderVersion(); }

EMSCRIPTEN_KEEPALIVE
uint8_t *create_buffer(size_t size) { return malloc(size * sizeof(uint8_t)); }

EMSCRIPTEN_KEEPALIVE
void destroy_buffer(uint8_t *p) { free(p); }

uintptr_t result[6];

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_pointer() { return result[0]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_size() { return result[1]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_xsize() { return result[2]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_ysize() { return result[3]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_icc() { return result[4]; }

void *alloc_func(void *opaque, size_t size) {
    return malloc(size * sizeof(uint8_t));
}

void free_func(void *opaque, void *address) { free(address); }

JxlMemoryManager memory_manager = {NULL, alloc_func, free_func};

EMSCRIPTEN_KEEPALIVE
void decode(const uint8_t *buffer, size_t size) {
    JxlDecoder *dec = JxlDecoderCreate(&memory_manager);

    JxlDecoderStatus status = JxlDecoderSubscribeEvents(
        dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE);
    assert(status == JXL_DEC_SUCCESS);

    const uint8_t *next_in = buffer;
    size_t avail_in = size;

    uint8_t *image_buffer;
    size_t output_buffer_size;
    size_t xsize, ysize;
    uint8_t *icc_profile;

    JxlBasicInfo info;
    JxlPixelFormat format = {4, JXL_TYPE_UINT8};
    size_t icc_size;

    for (;;) {
        status =
            JxlDecoderProcessInput(dec, (const uint8_t **)&next_in, &avail_in);
        switch (status) {
        case JXL_DEC_ERROR:
            fprintf(stderr, "Decoder error\n");
            return;
        case JXL_DEC_NEED_MORE_INPUT:
            fprintf(stderr, "Error, already provided all input\n");
            return;
        case JXL_DEC_BASIC_INFO:
            status = JxlDecoderGetBasicInfo(dec, &info);
            assert(status == JXL_DEC_SUCCESS);
            xsize = info.xsize;
            ysize = info.ysize;
            break;
        case JXL_DEC_COLOR_ENCODING:
            status = JxlDecoderGetICCProfileSize(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
            assert(status == JXL_DEC_SUCCESS);
            icc_profile = malloc(icc_size * sizeof(uint8_t));
            printf("ICC Size: %zuMiB\n", icc_size / 1024 / 1024);

            status = JxlDecoderGetColorAsICCProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_DATA, icc_profile,
                icc_size);
            assert(status == JXL_DEC_SUCCESS);
            break;
        case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
            status =
                JxlDecoderImageOutBufferSize(dec, &format, &output_buffer_size);
            assert(status == JXL_DEC_SUCCESS);
            image_buffer = malloc(output_buffer_size * sizeof(uint8_t));
            printf("Output Buffer Size: %zuMiB\n",
                   output_buffer_size / 1024 / 1024);

            status = JxlDecoderSetImageOutBuffer(
                dec, &format, (void *)image_buffer, output_buffer_size);
            assert(status == JXL_DEC_SUCCESS);
            break;
        case JXL_DEC_FULL_IMAGE:
            break;
        case JXL_DEC_SUCCESS:
            JxlDecoderDestroy(dec);

            result[0] = (uintptr_t)image_buffer;
            result[1] = output_buffer_size;
            result[2] = xsize;
            result[3] = ysize;
            result[4] = (uintptr_t)icc_profile;
            result[5] = icc_size;

            return;
        default:
            fprintf(stderr, "Unknown error: %d\n", status);
            return;
        }
    }
}

// int main() {
//     FILE *image = fopen("sample.jxl", "rb");
//     fseek(image, 0, SEEK_END);
//     size_t len = ftell(image);
//     rewind(image);

//     uint8_t *buffer = malloc(len);
//     fread(buffer, len, 1, image);
//     decode(buffer, len);
//     assert(result[1] / 4 == 2122 * 1433);
//     stbi_write_png("sample.png", result[2], result[3], 4,
//                    (const void *)result[0], result[2] * 4);
//     fclose(image);
//     return 0;
// }