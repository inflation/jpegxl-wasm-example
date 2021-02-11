#include "jxl/types.h"
#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/thread_parallel_runner.h>

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

EMSCRIPTEN_KEEPALIVE uintptr_t get_result_pointer() { return result[0]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_size() { return result[1]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_xsize() { return result[2]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_ysize() { return result[3]; }

EMSCRIPTEN_KEEPALIVE
uintptr_t get_result_icc() { return result[4]; }

EMSCRIPTEN_KEEPALIVE
void decode(const uint8_t *buffer, size_t size) {
    JxlDecoder *dec = JxlDecoderCreate(NULL);

    void *runner = JxlThreadParallelRunnerCreate(
        NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner)) {
        fprintf(stderr, "JxlDecoderSetParallelRunner failed\n");
        JxlThreadParallelRunnerDestroy(runner);
        JxlDecoderDestroy(dec);
        return;
    }

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

  JxlDecoderSetInput(dec, next_in, avail_in);

    for (;;) {
        status =
            JxlDecoderProcessInput(dec);
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

EMSCRIPTEN_KEEPALIVE
void encode(const uint8_t *buffer, size_t size, size_t xsize, size_t ysize,
            JXL_BOOL lossless, int effort, float quality) {
    JxlEncoder *enc = JxlEncoderCreate(NULL);

    JxlBasicInfo basic_info = {};
    basic_info.xsize = xsize;
    basic_info.ysize = ysize;
    basic_info.bits_per_sample = 8;
    basic_info.exponent_bits_per_sample = 0;
    basic_info.alpha_exponent_bits = 0;
    basic_info.alpha_bits = 8;
    basic_info.uses_original_profile = JXL_FALSE;
    if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc, &basic_info)) {
        fprintf(stderr, "JxlEncoderSetBasicInfo failed\n");
        JxlEncoderDestroy(enc);
        return;
    }

    JxlEncoderOptions *options = JxlEncoderOptionsCreate(enc, NULL);
    JxlEncoderOptionsSetLossless(options, lossless);
    JxlEncoderOptionsSetEffort(options, effort);
    JxlEncoderOptionsSetDistance(options, quality);

    JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    if (JXL_ENC_SUCCESS !=
        JxlEncoderAddImageFrame(options, &pixel_format, (void *)buffer, size)) {
        fprintf(stderr, "JxlEncoderAddImageFrame failed\n");
        JxlEncoderDestroy(enc);
        return;
    }

    size_t out_size = 512 * 1024; // 512 KiB
    uint8_t *compressed = malloc(out_size);

    uint8_t *next_out = compressed;
    size_t avail_out = out_size - (next_out - compressed);
    JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
    while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
        process_result = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
        if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t offset = next_out - compressed;
            out_size *= 2;
            compressed = realloc(compressed, out_size);

            next_out = compressed + offset;
            avail_out = out_size - offset;
        }
    }
    out_size = next_out - compressed;
    compressed = realloc(compressed, out_size);
    if (JXL_ENC_SUCCESS != process_result) {
        fprintf(stderr, "JxlEncoderProcessOutput failed\n");
        JxlEncoderDestroy(enc);
        return;
    }

    JxlEncoderDestroy(enc);

    result[0] = (uintptr_t)compressed;
    result[1] = out_size;
}

#ifndef __EMSCRIPTEN__
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
int main() {
    int x, y, n;
    uint8_t *data = stbi_load("assets/sample.png", &x, &y, &n, 4);
    if (!data)
        exit(1);
    encode(data, x * y * 4, x, y, 1, 3, 1.0);
    FILE *out = fopen("result.jxl", "wb");
    fwrite((const void *)result[0], 1, result[1], out);
    stbi_image_free(data);
    return 0;
}
#endif
