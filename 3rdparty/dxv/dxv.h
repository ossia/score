/*
 * Resolume DXV decompression (standalone, no ffmpeg dependencies)
 * Based on ffmpeg's dxv.c by Vittorio Giovara and Paul B Mahol
 *
 * Copyright (C) 2015 Vittorio Giovara <vittorio.giovara@gmail.com>
 * Copyright (C) 2018 Paul B Mahol
 *
 * LGPL v2.1+
 */

#ifndef DXV_H
#define DXV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DXV texture format tags (little-endian) */
enum DXVTextureFormat {
    DXV_FMT_DXT1 = 0x44585431, /* MKBETAG('D','X','T','1') */
    DXV_FMT_DXT5 = 0x44585435, /* MKBETAG('D','X','T','5') */
    DXV_FMT_YCG6 = 0x59434736, /* MKBETAG('Y','C','G','6') */
    DXV_FMT_YG10 = 0x59473130  /* MKBETAG('Y','G','1','0') */
};

/*
 * Decompression context.
 * Manages internal buffers for opcode-based decompression.
 * Must be initialized with dxv_ctx_init() and freed with dxv_ctx_free().
 */
typedef struct DXVDecompressContext {
    uint8_t *tex_data;
    unsigned tex_data_alloc;
    size_t tex_size;

    uint8_t *ctex_data;
    unsigned ctex_data_alloc;
    int64_t ctex_size;

    uint8_t *op_data[4];
    unsigned op_data_alloc[4];
    int64_t op_size[4];
} DXVDecompressContext;

void dxv_ctx_init(DXVDecompressContext *ctx);
void dxv_ctx_free(DXVDecompressContext *ctx);

/*
 * Decompress DXT1 DXTR-compressed data.
 * src/srcSize: compressed input (after 12-byte header)
 * dst: output buffer (must be pre-allocated to dstSize bytes)
 * dstSize: expected output size = (aligned_w/4) * (aligned_h/4) * 8
 * Returns 0 on success, negative on error.
 */
int dxv_decompress_dxt1(const uint8_t *src, int srcSize, uint8_t *dst, int dstSize);

/*
 * Decompress DXT5 DXTR-compressed data.
 * dstSize: expected output size = (aligned_w/4) * (aligned_h/4) * 16
 * Returns 0 on success, negative on error.
 */
int dxv_decompress_dxt5(const uint8_t *src, int srcSize, uint8_t *dst, int dstSize);

/*
 * Decompress LZF-compressed data (old DXV format).
 * dst: output buffer (pre-allocated to dstSize)
 * Returns number of bytes written on success, negative on error.
 */
int dxv_decompress_lzf(const uint8_t *src, int srcSize, uint8_t *dst, int dstSize);

/*
 * Decompress YCG6 format.
 * Produces BC4 blocks in tex_out (Y) and BC5 blocks in ctex_out (CoCg).
 *
 * ctx: decompression context (manages internal opcode buffers)
 * src/srcSize: compressed input (after 12-byte header)
 * tex_out: Y texture output (BC4), size = tex_out_size
 * ctex_out: CoCg texture output (BC5 = interleaved BC4), size = ctex_out_size
 * coded_w, coded_h: dimensions aligned to 4
 *
 * Returns 0 on success, negative on error.
 */
int dxv_decompress_ycg6(
    DXVDecompressContext *ctx,
    const uint8_t *src, int srcSize,
    uint8_t *tex_out, int tex_out_size,
    uint8_t *ctex_out, int ctex_out_size,
    int coded_w, int coded_h);

/*
 * Decompress YG10 format.
 * Produces BC5 blocks in tex_out (Y+alpha) and BC5 blocks in ctex_out (CoCg).
 *
 * Returns 0 on success, negative on error.
 */
int dxv_decompress_yg10(
    DXVDecompressContext *ctx,
    const uint8_t *src, int srcSize,
    uint8_t *tex_out, int tex_out_size,
    uint8_t *ctex_out, int ctex_out_size,
    int coded_w, int coded_h);

#ifdef __cplusplus
}
#endif

#endif /* DXV_H */
