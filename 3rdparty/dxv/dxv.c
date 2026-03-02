/*
 * Resolume DXV decompression (standalone, no ffmpeg dependencies)
 * Based on ffmpeg's dxv.c by Vittorio Giovara and Paul B Mahol
 *
 * Copyright (C) 2015 Vittorio Giovara <vittorio.giovara@gmail.com>
 * Copyright (C) 2018 Paul B Mahol
 *
 * LGPL v2.1+
 */

#include "dxv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================== */
/* Byte stream reader (replaces ffmpeg's GetByteContext)                       */
/* ========================================================================== */

typedef struct ByteStream {
    const uint8_t *buffer; /* start of data */
    const uint8_t *ptr;    /* current read position */
    const uint8_t *end;    /* end of data */
} ByteStream;

static void bs_init(ByteStream *bs, const uint8_t *data, int size)
{
    bs->buffer = data;
    bs->ptr = data;
    bs->end = data + size;
}

static int bs_left(const ByteStream *bs)
{
    return (int)(bs->end - bs->ptr);
}

static int bs_tell(const ByteStream *bs)
{
    return (int)(bs->ptr - bs->buffer);
}

static void bs_seek(ByteStream *bs, int offset, int whence)
{
    if (whence == SEEK_SET)
        bs->ptr = bs->buffer + offset;
    else if (whence == SEEK_CUR)
        bs->ptr = bs->ptr + offset;

    if (bs->ptr < bs->buffer) bs->ptr = bs->buffer;
    if (bs->ptr > bs->end) bs->ptr = bs->end;
}

static void bs_skip(ByteStream *bs, int n)
{
    bs->ptr += n;
    if (bs->ptr > bs->end) bs->ptr = bs->end;
    if (bs->ptr < bs->buffer) bs->ptr = bs->buffer;
}

static uint8_t bs_get_byte(ByteStream *bs)
{
    if (bs->ptr >= bs->end) return 0;
    return *bs->ptr++;
}

static uint8_t bs_peek_byte(const ByteStream *bs)
{
    if (bs->ptr >= bs->end) return 0;
    return *bs->ptr;
}

static uint16_t bs_get_le16(ByteStream *bs)
{
    if (bs->ptr + 2 > bs->end) return 0;
    uint16_t v = bs->ptr[0] | ((uint16_t)bs->ptr[1] << 8);
    bs->ptr += 2;
    return v;
}

static uint32_t bs_get_le32(ByteStream *bs)
{
    if (bs->ptr + 4 > bs->end) return 0;
    uint32_t v = bs->ptr[0] | ((uint32_t)bs->ptr[1] << 8) |
                 ((uint32_t)bs->ptr[2] << 16) | ((uint32_t)bs->ptr[3] << 24);
    bs->ptr += 4;
    return v;
}

static int bs_get_buffer(ByteStream *bs, uint8_t *dst, int size)
{
    int avail = (int)(bs->end - bs->ptr);
    if (avail < size) size = avail;
    if (size > 0) {
        memcpy(dst, bs->ptr, size);
        bs->ptr += size;
    }
    return size;
}

/* ========================================================================== */
/* Endian read/write helpers (replaces AV_RL32 etc.)                          */
/* ========================================================================== */

static inline uint16_t rl16(const uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t rl32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void wl16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static inline void wl32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/* Count leading zeros (replaces ff_clz) */
static inline int clz32(unsigned v)
{
    if (v == 0) return 32;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clz(v);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse(&idx, v);
    return 31 - (int)idx;
#else
    int n = 0;
    if (v <= 0x0000FFFF) { n += 16; v <<= 16; }
    if (v <= 0x00FFFFFF) { n += 8;  v <<= 8;  }
    if (v <= 0x0FFFFFFF) { n += 4;  v <<= 4;  }
    if (v <= 0x3FFFFFFF) { n += 2;  v <<= 2;  }
    if (v <= 0x7FFFFFFF) { n += 1; }
    return n;
#endif
}

/* Overlapping backward copy (replaces av_memcpy_backptr) */
static void memcpy_backptr(uint8_t *dst, int back, int cnt)
{
    const uint8_t *src = dst - back;
    for (int i = 0; i < cnt; i++)
        dst[i] = src[i];
}

/* ========================================================================== */
/* LZF decompression (replaces ffmpeg lzf.c)                                  */
/* ========================================================================== */

#define LZF_LITERAL_MAX 32
#define LZF_LONG_BACKREF (7 + 2)

int dxv_decompress_lzf(const uint8_t *src, int srcSize, uint8_t *dst, int dstSize)
{
    ByteStream _bs, *bs = &_bs;
    bs_init(bs, src, srcSize);
    int len = 0;

    while (bs_left(bs) > 2) {
        uint8_t s = bs_get_byte(bs);

        if (s < LZF_LITERAL_MAX) {
            int count = s + 1;
            if (len + count > dstSize)
                return -1;
            int rd = bs_get_buffer(bs, dst + len, count);
            if (rd != count)
                return -1;
            len += count;
        } else {
            int l = 2 + (s >> 5);
            int off = ((s & 0x1f) << 8) + 1;

            if (l == LZF_LONG_BACKREF)
                l += bs_get_byte(bs);
            off += bs_get_byte(bs);

            if (off > len) return -1;
            if (len + l > dstSize) return -1;

            memcpy_backptr(dst + len, off, l);
            len += l;
        }
    }
    return len;
}

/* ========================================================================== */
/* DXTR DXT1 decompression                                                    */
/* ========================================================================== */

#define CHECKPOINT(x, gbc, avctx)                                             \
    do {                                                                      \
        if (state == 0) {                                                     \
            if (bs_left(gbc) < 4)                                             \
                return -1;                                                    \
            value = bs_get_le32(gbc);                                         \
            state = 16;                                                       \
        }                                                                     \
        op = value & 0x3;                                                     \
        value >>= 2;                                                          \
        state--;                                                              \
        switch (op) {                                                         \
        case 1:                                                               \
            idx = x;                                                          \
            break;                                                            \
        case 2:                                                               \
            idx = (bs_get_byte(gbc) + 2) * x;                                 \
            if (idx > pos)                                                    \
                return -1;                                                    \
            break;                                                            \
        case 3:                                                               \
            idx = (bs_get_le16(gbc) + 0x102) * x;                             \
            if (idx > pos)                                                    \
                return -1;                                                    \
            break;                                                            \
        }                                                                     \
    } while(0)

int dxv_decompress_dxt1(const uint8_t *src, int srcSize, uint8_t *dst, int dstSize)
{
    ByteStream _gbc, *gbc = &_gbc;
    bs_init(gbc, src, srcSize);

    uint32_t value, prev, op;
    int idx = 0, state = 0;
    int pos = 2;
    int maxPos = dstSize / 4;

    wl32(dst, bs_get_le32(gbc));
    wl32(dst + 4, bs_get_le32(gbc));

    while (pos + 2 <= maxPos) {
        CHECKPOINT(2, gbc, NULL);

        if (op) {
            if (pos - idx < 0) return -1;
            prev = rl32(dst + 4 * (pos - idx));
            wl32(dst + 4 * pos, prev);
            pos++;
            prev = rl32(dst + 4 * (pos - idx));
            wl32(dst + 4 * pos, prev);
            pos++;
        } else {
            CHECKPOINT(2, gbc, NULL);
            if (op)
                prev = rl32(dst + 4 * (pos - idx));
            else
                prev = bs_get_le32(gbc);
            wl32(dst + 4 * pos, prev);
            pos++;

            CHECKPOINT(2, gbc, NULL);
            if (op)
                prev = rl32(dst + 4 * (pos - idx));
            else
                prev = bs_get_le32(gbc);
            wl32(dst + 4 * pos, prev);
            pos++;
        }
    }
    return 0;
}

/* ========================================================================== */
/* DXTR DXT5 decompression                                                    */
/* ========================================================================== */

int dxv_decompress_dxt5(const uint8_t *src, int srcSize, uint8_t *dst, int dstSize)
{
    ByteStream _gbc, *gbc = &_gbc;
    bs_init(gbc, src, srcSize);

    uint32_t value, op, prev;
    int idx = 0, state = 0;
    int pos = 4;
    int run = 0;
    int probe, check;
    int maxPos = dstSize / 4;

    wl32(dst + 0,  bs_get_le32(gbc));
    wl32(dst + 4,  bs_get_le32(gbc));
    wl32(dst + 8,  bs_get_le32(gbc));
    wl32(dst + 12, bs_get_le32(gbc));

    while (pos + 2 <= maxPos) {
        if (run) {
            run--;
            prev = rl32(dst + 4 * (pos - 4));
            wl32(dst + 4 * pos, prev); pos++;
            prev = rl32(dst + 4 * (pos - 4));
            wl32(dst + 4 * pos, prev); pos++;
        } else {
            if (bs_left(gbc) < 1) return -1;
            if (state == 0) {
                value = bs_get_le32(gbc);
                state = 16;
            }
            op = value & 0x3;
            value >>= 2;
            state--;

            switch (op) {
            case 0:
                check = bs_get_byte(gbc) + 1;
                if (check == 256) {
                    do {
                        probe = bs_get_le16(gbc);
                        check += probe;
                    } while (probe == 0xFFFF);
                }
                while (check && pos + 4 <= maxPos) {
                    prev = rl32(dst + 4 * (pos - 4)); wl32(dst + 4 * pos, prev); pos++;
                    prev = rl32(dst + 4 * (pos - 4)); wl32(dst + 4 * pos, prev); pos++;
                    prev = rl32(dst + 4 * (pos - 4)); wl32(dst + 4 * pos, prev); pos++;
                    prev = rl32(dst + 4 * (pos - 4)); wl32(dst + 4 * pos, prev); pos++;
                    check--;
                }
                continue;
            case 1:
                run = bs_get_byte(gbc);
                if (run == 255) {
                    do {
                        probe = bs_get_le16(gbc);
                        run += probe;
                    } while (probe == 0xFFFF);
                }
                prev = rl32(dst + 4 * (pos - 4)); wl32(dst + 4 * pos, prev); pos++;
                prev = rl32(dst + 4 * (pos - 4)); wl32(dst + 4 * pos, prev); pos++;
                break;
            case 2:
                idx = 8 + 4 * bs_get_le16(gbc);
                if (idx > pos || (unsigned)(pos - idx) + 2 > (unsigned)maxPos)
                    return -1;
                prev = rl32(dst + 4 * (pos - idx)); wl32(dst + 4 * pos, prev); pos++;
                prev = rl32(dst + 4 * (pos - idx)); wl32(dst + 4 * pos, prev); pos++;
                break;
            case 3:
                prev = bs_get_le32(gbc); wl32(dst + 4 * pos, prev); pos++;
                prev = bs_get_le32(gbc); wl32(dst + 4 * pos, prev); pos++;
                break;
            }
        }

        CHECKPOINT(4, gbc, NULL);
        if (pos + 2 > maxPos) break;

        if (op) {
            if (idx > pos || (unsigned)(pos - idx) + 2 > (unsigned)maxPos)
                return -1;
            prev = rl32(dst + 4 * (pos - idx)); wl32(dst + 4 * pos, prev); pos++;
            prev = rl32(dst + 4 * (pos - idx)); wl32(dst + 4 * pos, prev); pos++;
        } else {
            CHECKPOINT(4, gbc, NULL);
            if (op && (idx > pos || (unsigned)(pos - idx) + 2 > (unsigned)maxPos))
                return -1;
            if (op) prev = rl32(dst + 4 * (pos - idx));
            else    prev = bs_get_le32(gbc);
            wl32(dst + 4 * pos, prev); pos++;

            CHECKPOINT(4, gbc, NULL);
            if (op) prev = rl32(dst + 4 * (pos - idx));
            else    prev = bs_get_le32(gbc);
            wl32(dst + 4 * pos, prev); pos++;
        }
    }
    return 0;
}

#undef CHECKPOINT

/* ========================================================================== */
/* Opcode decompression for YCG6/YG10 (Huffman + CGO opcodes)                */
/* ========================================================================== */

typedef struct OpcodeTable {
    int16_t next;
    uint8_t val1;
    uint8_t val2;
} OpcodeTable;

static int fill_ltable(ByteStream *bs, uint32_t *table, int *nb_elements)
{
    unsigned half = 512, bits = 1023, left = 1024, input, mask;
    int value, counter = 0, rshift = 10, lshift = 30;

    mask = bs_get_le32(bs) >> 2;
    while (left) {
        if (counter >= 256) return -1;
        value = bits & mask;
        left -= bits & mask;
        mask >>= rshift;
        lshift -= rshift;
        table[counter++] = value;
        if (lshift < 16) {
            if (bs_left(bs) <= 0) return -1;
            input = bs_get_le16(bs);
            mask += input << lshift;
            lshift += 16;
        }
        if (left < half) {
            half >>= 1;
            bits >>= 1;
            rshift--;
        }
    }

    for (; !table[counter - 1]; counter--)
        if (counter <= 0) return -1;

    *nb_elements = counter;
    if (counter < 256)
        memset(&table[counter], 0, 4 * (256 - counter));

    if (lshift >= 16)
        bs_seek(bs, -2, SEEK_CUR);

    return 0;
}

static int fill_optable(unsigned *table0, OpcodeTable *table1, int nb_elements)
{
    unsigned table2[256] = { 0 };
    unsigned x = 0;
    int val0, val1, i, j = 2, k = 0;

    table2[0] = table0[0];
    for (i = 0; i < nb_elements - 1; i++, table2[i] = val0)
        val0 = table0[i + 1] + table2[i];

    if (!table2[0]) {
        do { k++; } while (!table2[k]);
    }

    j = 2;
    for (i = 1024; i > 0; i--) {
        for (table1[x].val1 = k; k < 256 && j > (int)table2[k]; k++);
        x = (x - 383) & 0x3FF;
        j++;
    }

    if (nb_elements > 0)
        memcpy(&table2[0], table0, 4 * nb_elements);

    for (i = 0; i < 1024; i++) {
        val0 = table1[i].val1;
        val1 = table2[val0];
        table2[val0]++;
        x = 31 - clz32(val1);
        if (x > 10) return -1;
        table1[i].val2 = 10 - x;
        table1[i].next = (val1 << table1[i].val2) - 1024;
    }
    return 0;
}

static int get_opcodes(ByteStream *bs, uint32_t *table, uint8_t *dst,
                       int op_size, int nb_elements)
{
    OpcodeTable optable[1024];
    int sum, x, val, lshift, rshift, ret, i, idx;
    int64_t size_in_bits;
    unsigned endoffset, newoffset, offset, next;
    const uint8_t *src = bs->buffer;

    ret = fill_optable(table, optable, nb_elements);
    if (ret < 0) return ret;

    size_in_bits = bs_get_le32(bs);
    endoffset = (unsigned)(((size_in_bits + 7) >> 3) - 4);
    if ((int)endoffset <= 0 || bs_left(bs) < (int)endoffset)
        return -1;

    offset = endoffset;
    next = rl32(src + endoffset);
    rshift = (((size_in_bits & 0xFF) - 1) & 7) + 15;
    lshift = 32 - rshift;
    idx = (next >> rshift) & 0x3FF;

    for (i = 0; i < op_size; i++) {
        dst[i] = optable[idx].val1;
        val = optable[idx].val2;
        sum = val + lshift;
        x = (next << lshift) >> 1 >> (31 - val);
        newoffset = offset - (sum >> 3);
        lshift = sum & 7;
        idx = x + optable[idx].next;
        offset = newoffset;
        if (offset > endoffset) return -1;
        next = rl32(src + offset);
    }

    bs_skip(bs, (int)((size_in_bits + 7) >> 3) - 4);
    return 0;
}

static int decompress_opcodes(ByteStream *bs, void *dstp, size_t op_size)
{
    int pos = bs_tell(bs);
    int flag = bs_peek_byte(bs);

    if ((flag & 3) == 0) {
        bs_skip(bs, 1);
        int rd = bs_get_buffer(bs, (uint8_t*)dstp, (int)op_size);
        if (rd != (int)op_size) return -1;
    } else if ((flag & 3) == 1) {
        bs_skip(bs, 1);
        memset(dstp, bs_get_byte(bs), op_size);
    } else {
        uint32_t table[256];
        int ret, elements = 0;
        ret = fill_ltable(bs, table, &elements);
        if (ret < 0) return ret;
        ret = get_opcodes(bs, table, (uint8_t*)dstp, (int)op_size, elements);
        if (ret < 0) return ret;
    }
    return bs_tell(bs) - pos;
}

/* ========================================================================== */
/* CGO decompression (18 opcodes with hash tables)                            */
/* ========================================================================== */

static int decompress_cgo(ByteStream *bs,
                          uint8_t *tex_data, int tex_size,
                          uint8_t *op_data, int *oindex, int op_size,
                          uint8_t **dstp, int *statep,
                          uint8_t **tab0, uint8_t **tab1,
                          int offset)
{
    uint8_t *dst = *dstp;
    uint8_t *tptr0, *tptr1, *tptr3;
    int oi = *oindex;
    int state = *statep;
    int opcode, v, vv;

    if (state <= 0) {
        if (oi >= op_size) return -1;
        opcode = op_data[oi++];
        if (!opcode) {
            v = bs_get_byte(bs);
            if (v == 255) {
                do {
                    if (bs_left(bs) <= 0) return -1;
                    opcode = bs_get_le16(bs);
                    v += opcode;
                } while (opcode == 0xFFFF);
            }
            wl32(dst, rl32(dst - (8 + offset)));
            wl32(dst + 4, rl32(dst - (4 + offset)));
            state = v + 4;
            goto done;
        }

        switch (opcode) {
        case 1:
            wl32(dst, rl32(dst - (8 + offset)));
            wl32(dst + 4, rl32(dst - (4 + offset)));
            break;
        case 2:
            vv = (8 + offset) * (bs_get_le16(bs) + 1);
            if (vv < 0 || vv > (int)(dst - tex_data)) return -1;
            tptr0 = dst - vv;
            v = rl32(tptr0);
            wl32(dst, rl32(tptr0));
            wl32(dst + 4, rl32(tptr0 + 4));
            tab0[(unsigned)(0x9E3779B1u * (uint16_t)v) >> 24] = dst;
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 3:
            wl32(dst, bs_get_le32(bs));
            wl32(dst + 4, bs_get_le32(bs));
            tab0[(unsigned)(0x9E3779B1u * rl16(dst)) >> 24] = dst;
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 4:
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            wl16(dst, bs_get_le16(bs));
            wl16(dst + 2, rl16(tptr3));
            dst[4] = tptr3[2];
            wl16(dst + 5, bs_get_le16(bs));
            dst[7] = bs_get_byte(bs);
            tab0[(unsigned)(0x9E3779B1u * rl16(dst)) >> 24] = dst;
            break;
        case 5:
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            wl16(dst, bs_get_le16(bs));
            wl16(dst + 2, bs_get_le16(bs));
            dst[4] = bs_get_byte(bs);
            wl16(dst + 5, rl16(tptr3));
            dst[7] = tptr3[2];
            tab0[(unsigned)(0x9E3779B1u * rl16(dst)) >> 24] = dst;
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 6:
            tptr0 = tab1[bs_get_byte(bs)];
            if (!tptr0) return -1;
            tptr1 = tab1[bs_get_byte(bs)];
            if (!tptr1) return -1;
            wl16(dst, bs_get_le16(bs));
            wl16(dst + 2, rl16(tptr0));
            dst[4] = tptr0[2];
            wl16(dst + 5, rl16(tptr1));
            dst[7] = tptr1[2];
            tab0[(unsigned)(0x9E3779B1u * rl16(dst)) >> 24] = dst;
            break;
        case 7:
            v = (8 + offset) * (bs_get_le16(bs) + 1);
            if (v < 0 || v > (int)(dst - tex_data)) return -1;
            tptr0 = dst - v;
            wl16(dst, bs_get_le16(bs));
            wl16(dst + 2, rl16(tptr0 + 2));
            wl32(dst + 4, rl32(tptr0 + 4));
            tab0[(unsigned)(0x9E3779B1u * rl16(dst)) >> 24] = dst;
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 8:
            tptr1 = tab0[bs_get_byte(bs)];
            if (!tptr1) return -1;
            wl16(dst, rl16(tptr1));
            wl16(dst + 2, bs_get_le16(bs));
            wl32(dst + 4, bs_get_le32(bs));
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 9:
            tptr1 = tab0[bs_get_byte(bs)];
            if (!tptr1) return -1;
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            wl16(dst, rl16(tptr1));
            wl16(dst + 2, rl16(tptr3));
            dst[4] = tptr3[2];
            wl16(dst + 5, bs_get_le16(bs));
            dst[7] = bs_get_byte(bs);
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 10:
            tptr1 = tab0[bs_get_byte(bs)];
            if (!tptr1) return -1;
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            wl16(dst, rl16(tptr1));
            wl16(dst + 2, bs_get_le16(bs));
            dst[4] = bs_get_byte(bs);
            wl16(dst + 5, rl16(tptr3));
            dst[7] = tptr3[2];
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 11:
            tptr0 = tab0[bs_get_byte(bs)];
            if (!tptr0) return -1;
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            tptr1 = tab1[bs_get_byte(bs)];
            if (!tptr1) return -1;
            wl16(dst, rl16(tptr0));
            wl16(dst + 2, rl16(tptr3));
            dst[4] = tptr3[2];
            wl16(dst + 5, rl16(tptr1));
            dst[7] = tptr1[2];
            break;
        case 12:
            tptr1 = tab0[bs_get_byte(bs)];
            if (!tptr1) return -1;
            v = (8 + offset) * (bs_get_le16(bs) + 1);
            if (v < 0 || v > (int)(dst - tex_data)) return -1;
            tptr0 = dst - v;
            wl16(dst, rl16(tptr1));
            wl16(dst + 2, rl16(tptr0 + 2));
            wl32(dst + 4, rl32(tptr0 + 4));
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 13:
            wl16(dst, rl16(dst - (8 + offset)));
            wl16(dst + 2, bs_get_le16(bs));
            wl32(dst + 4, bs_get_le32(bs));
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 14:
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            wl16(dst, rl16(dst - (8 + offset)));
            wl16(dst + 2, rl16(tptr3));
            dst[4] = tptr3[2];
            wl16(dst + 5, bs_get_le16(bs));
            dst[7] = bs_get_byte(bs);
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 15:
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            wl16(dst, rl16(dst - (8 + offset)));
            wl16(dst + 2, bs_get_le16(bs));
            dst[4] = bs_get_byte(bs);
            wl16(dst + 5, rl16(tptr3));
            dst[7] = tptr3[2];
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        case 16:
            tptr3 = tab1[bs_get_byte(bs)];
            if (!tptr3) return -1;
            tptr1 = tab1[bs_get_byte(bs)];
            if (!tptr1) return -1;
            wl16(dst, rl16(dst - (8 + offset)));
            wl16(dst + 2, rl16(tptr3));
            dst[4] = tptr3[2];
            wl16(dst + 5, rl16(tptr1));
            dst[7] = tptr1[2];
            break;
        case 17:
            v = (8 + offset) * (bs_get_le16(bs) + 1);
            if (v < 0 || v > (int)(dst - tex_data)) return -1;
            wl16(dst, rl16(dst - (8 + offset)));
            wl16(dst + 2, rl16(&dst[-v + 2]));
            wl32(dst + 4, rl32(&dst[-v + 4]));
            tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFFu)) >> 24] = dst + 2;
            break;
        default:
            break;
        }
    } else {
done:
        wl32(dst, rl32(dst - (8 + offset)));
        wl32(dst + 4, rl32(dst - (4 + offset)));
        state--;
    }
    if (dst - tex_data + 8 > tex_size)
        return -1;
    dst += 8;

    *oindex = oi;
    *dstp = dst;
    *statep = state;
    return 0;
}

/* ========================================================================== */
/* CoCg decompression (two interleaved CGO streams)                           */
/* ========================================================================== */

static int decompress_cocg(ByteStream *bs,
                           uint8_t *tex_data, int tex_size,
                           uint8_t *op_data0, uint8_t *op_data1,
                           int max_op_size0, int max_op_size1)
{
    uint8_t *dst;
    uint8_t *tab2[256] = { 0 }, *tab0[256] = { 0 };
    uint8_t *tab3[256] = { 0 }, *tab1[256] = { 0 };

    int op_offset = bs_get_le32(bs);
    unsigned op_size0 = bs_get_le32(bs);
    unsigned op_size1 = bs_get_le32(bs);
    int data_start = bs_tell(bs);
    int skip0, skip1, oi0 = 0, oi1 = 0;
    int ret, state0 = 0, state1 = 0;

    if (op_offset < 12 || op_offset - 12 > bs_left(bs))
        return -1;

    dst = tex_data;
    bs_skip(bs, op_offset - 12);

    if ((int)op_size0 > max_op_size0) return -1;
    skip0 = decompress_opcodes(bs, op_data0, op_size0);
    if (skip0 < 0) return skip0;

    if ((int)op_size1 > max_op_size1) return -1;
    skip1 = decompress_opcodes(bs, op_data1, op_size1);
    if (skip1 < 0) return skip1;

    bs_seek(bs, data_start, SEEK_SET);

    wl32(dst, bs_get_le32(bs));
    wl32(dst + 4, bs_get_le32(bs));
    wl32(dst + 8, bs_get_le32(bs));
    wl32(dst + 12, bs_get_le32(bs));

    tab0[(unsigned)(0x9E3779B1u * rl16(dst)) >> 24] = dst;
    tab1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFF)) >> 24] = dst + 2;
    tab2[(unsigned)(0x9E3779B1u * rl16(dst + 8)) >> 24] = dst + 8;
    tab3[(unsigned)(0x9E3779B1u * (rl32(dst + 10) & 0xFFFFFF)) >> 24] = dst + 10;
    dst += 16;

    while (dst + 10 < tex_data + tex_size) {
        ret = decompress_cgo(bs, tex_data, tex_size, op_data0, &oi0, op_size0,
                             &dst, &state0, tab0, tab1, 8);
        if (ret < 0) return ret;
        ret = decompress_cgo(bs, tex_data, tex_size, op_data1, &oi1, op_size1,
                             &dst, &state1, tab2, tab3, 8);
        if (ret < 0) return ret;
    }

    bs_seek(bs, data_start - 12 + op_offset + skip0 + skip1, SEEK_SET);
    return 0;
}

/* ========================================================================== */
/* YO decompression (single CGO stream for Y luma)                            */
/* ========================================================================== */

static int decompress_yo(ByteStream *bs,
                         uint8_t *tex_data, int tex_size,
                         uint8_t *op_data, int max_op_size)
{
    int op_offset = bs_get_le32(bs);
    unsigned op_size = bs_get_le32(bs);
    int data_start = bs_tell(bs);
    uint8_t *dst;
    uint8_t *table0[256] = { 0 }, *table1[256] = { 0 };
    int ret, state = 0, skip, oi = 0;
    uint32_t v, vv;

    if (op_offset < 8 || op_offset - 8 > bs_left(bs))
        return -1;

    dst = tex_data;
    bs_skip(bs, op_offset - 8);

    if ((int)op_size > max_op_size) return -1;
    skip = decompress_opcodes(bs, op_data, op_size);
    if (skip < 0) return skip;

    bs_seek(bs, data_start, SEEK_SET);

    v = bs_get_le32(bs);
    wl32(dst, v);
    vv = bs_get_le32(bs);
    table0[(unsigned)(0x9E3779B1u * (uint16_t)v) >> 24] = dst;
    wl32(dst + 4, vv);
    table1[(unsigned)(0x9E3779B1u * (rl32(dst + 2) & 0xFFFFFF)) >> 24] = dst + 2;
    dst += 8;

    while (dst < tex_data + tex_size) {
        ret = decompress_cgo(bs, tex_data, tex_size, op_data, &oi, op_size,
                             &dst, &state, table0, table1, 0);
        if (ret < 0) return ret;
    }

    bs_seek(bs, data_start + op_offset + skip - 8, SEEK_SET);
    return 0;
}

/* ========================================================================== */
/* Context management                                                         */
/* ========================================================================== */

void dxv_ctx_init(DXVDecompressContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void dxv_ctx_free(DXVDecompressContext *ctx)
{
    free(ctx->tex_data);
    ctx->tex_data = NULL;
    free(ctx->ctex_data);
    ctx->ctex_data = NULL;
    for (int i = 0; i < 4; i++) {
        free(ctx->op_data[i]);
        ctx->op_data[i] = NULL;
    }
}

static int ensure_buf(uint8_t **buf, unsigned *alloc, size_t needed)
{
    if (*alloc >= needed) return 0;
    size_t new_size = needed + 1024;
    uint8_t *p = (uint8_t*)realloc(*buf, new_size);
    if (!p) return -1;
    *buf = p;
    *alloc = (unsigned)new_size;
    return 0;
}

/* ========================================================================== */
/* YCG6 decompression: BC4 (Y) + interleaved BC4 (CoCg)                      */
/* ========================================================================== */

int dxv_decompress_ycg6(
    DXVDecompressContext *ctx,
    const uint8_t *src, int srcSize,
    uint8_t *tex_out, int tex_out_size,
    uint8_t *ctex_out, int ctex_out_size,
    int coded_w, int coded_h)
{
    ByteStream _bs, *bs = &_bs;
    bs_init(bs, src, srcSize);
    int ret;

    /* Compute opcode buffer sizes */
    ctx->op_size[0] = coded_w * coded_h / 16;
    ctx->op_size[1] = coded_w * coded_h / 32;
    ctx->op_size[2] = coded_w * coded_h / 32;

    /* Ensure internal buffers */
    ctx->tex_size = tex_out_size;
    if (ensure_buf(&ctx->tex_data, &ctx->tex_data_alloc, tex_out_size + 64) < 0)
        return -1;
    ctx->ctex_size = ctex_out_size;
    if (ensure_buf(&ctx->ctex_data, &ctx->ctex_data_alloc, ctex_out_size + 64) < 0)
        return -1;
    for (int i = 0; i < 3; i++) {
        if (ensure_buf(&ctx->op_data[i], &ctx->op_data_alloc[i], ctx->op_size[i]) < 0)
            return -1;
    }

    /* Decompress Y (BC4 blocks) */
    ret = decompress_yo(bs, ctx->tex_data, ctx->tex_size,
                        ctx->op_data[0], ctx->op_size[0]);
    if (ret < 0) return ret;

    /* Decompress CoCg (interleaved BC4 = BC5 blocks) */
    ret = decompress_cocg(bs, ctx->ctex_data, ctx->ctex_size,
                          ctx->op_data[1], ctx->op_data[2],
                          ctx->op_size[1], ctx->op_size[2]);
    if (ret < 0) return ret;

    /* Copy decompressed data to output */
    memcpy(tex_out, ctx->tex_data, tex_out_size);
    memcpy(ctex_out, ctx->ctex_data, ctex_out_size);

    return 0;
}

/* ========================================================================== */
/* YG10 decompression: BC5 (Y+alpha) + interleaved BC4 (CoCg)                */
/* ========================================================================== */

int dxv_decompress_yg10(
    DXVDecompressContext *ctx,
    const uint8_t *src, int srcSize,
    uint8_t *tex_out, int tex_out_size,
    uint8_t *ctex_out, int ctex_out_size,
    int coded_w, int coded_h)
{
    ByteStream _bs, *bs = &_bs;
    bs_init(bs, src, srcSize);
    int ret;

    /* Compute opcode buffer sizes */
    ctx->op_size[0] = coded_w * coded_h / 16;
    ctx->op_size[1] = coded_w * coded_h / 32;
    ctx->op_size[2] = coded_w * coded_h / 32;
    ctx->op_size[3] = coded_w * coded_h / 16;

    /* Ensure internal buffers */
    ctx->tex_size = tex_out_size;
    if (ensure_buf(&ctx->tex_data, &ctx->tex_data_alloc, tex_out_size + 64) < 0)
        return -1;
    ctx->ctex_size = ctex_out_size;
    if (ensure_buf(&ctx->ctex_data, &ctx->ctex_data_alloc, ctex_out_size + 64) < 0)
        return -1;
    for (int i = 0; i < 4; i++) {
        if (ensure_buf(&ctx->op_data[i], &ctx->op_data_alloc[i], ctx->op_size[i]) < 0)
            return -1;
    }

    /* Decompress Y+alpha (BC5 blocks, two interleaved CoCg streams) */
    ret = decompress_cocg(bs, ctx->tex_data, ctx->tex_size,
                          ctx->op_data[0], ctx->op_data[3],
                          ctx->op_size[0], ctx->op_size[3]);
    if (ret < 0) return ret;

    /* Decompress CoCg (interleaved BC4 = BC5 blocks) */
    ret = decompress_cocg(bs, ctx->ctex_data, ctx->ctex_size,
                          ctx->op_data[1], ctx->op_data[2],
                          ctx->op_size[1], ctx->op_size[2]);
    if (ret < 0) return ret;

    /* Copy decompressed data to output */
    memcpy(tex_out, ctx->tex_data, tex_out_size);
    memcpy(ctex_out, ctx->ctex_data, ctex_out_size);

    return 0;
}
