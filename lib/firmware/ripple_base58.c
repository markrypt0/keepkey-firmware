/**
 * Copyright (c) 2012-2014 Luke Dashjr
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "keepkey/firmware/ripple_base58.h"

#include "hwcrypto/crypto/memzero.h"
#include "hwcrypto/crypto/ripemd160.h"
#include "hwcrypto/crypto/sha2.h"

#include <stdbool.h>
#include <string.h>

// https://developers.ripple.com/base58-encodings.html
const char ripple_b58digits_ordered[] =
    "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz";
const int8_t ripple_b58digits_map[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 50, 33, 7,  21, 41, 40, 27, 45,
    8,  -1, -1, -1, -1, -1, -1, -1, 54, 10, 38, 12, 14, 47, 15, 16, -1, 17, 18,
    19, 20, 13, -1, 22, 23, 24, 25, 26, 11, 28, 29, 30, 31, 32, -1, -1, -1, -1,
    -1, -1, 5,  34, 35, 36, 37, 6,  39, 3,  49, 42, 43, -1, 44, 4,  46, 1,  48,
    0,  2,  51, 52, 53, 9,  55, 56, 57, -1, -1, -1, -1, -1};

typedef uint64_t b58_maxint_t;
typedef uint32_t b58_almostmaxint_t;
#define b58_almostmaxint_bits (sizeof(b58_almostmaxint_t) * 8)
static const b58_almostmaxint_t b58_almostmaxint_mask =
    ((((b58_maxint_t)1) << b58_almostmaxint_bits) - 1);

bool ripple_b58tobin(void *bin, size_t *binszp, const char *b58) {
  size_t binsz = *binszp;

  if (binsz == 0) {
    return false;
  }

  const unsigned char *b58u = (const unsigned char *)b58;
  unsigned char *binu = bin;
  size_t outisz =
      (binsz + sizeof(b58_almostmaxint_t) - 1) / sizeof(b58_almostmaxint_t);
  b58_almostmaxint_t outi[outisz];
  b58_maxint_t t = 0;
  b58_almostmaxint_t c = 0;
  size_t i = 0, j = 0;
  uint8_t bytesleft = binsz % sizeof(b58_almostmaxint_t);
  b58_almostmaxint_t zeromask =
      bytesleft ? (b58_almostmaxint_mask << (bytesleft * 8)) : 0;
  unsigned zerocount = 0;

  size_t b58sz = strlen(b58);

  memzero(outi, sizeof(outi));

  // Leading zeros, just count
  for (i = 0; i < b58sz && b58u[i] == 'r'; ++i) ++zerocount;

  for (; i < b58sz; ++i) {
    if (b58u[i] & 0x80)
      // High-bit set on invalid digit
      return false;
    if (ripple_b58digits_map[b58u[i]] == -1)
      // Invalid base58 digit
      return false;
    c = (unsigned)ripple_b58digits_map[b58u[i]];
    for (j = outisz; j--;) {
      t = ((b58_maxint_t)outi[j]) * 58 + c;
      c = t >> b58_almostmaxint_bits;
      outi[j] = t & b58_almostmaxint_mask;
    }
    if (c)
      // Output number too big (carry to the next int32)
      return false;
    if (outi[0] & zeromask)
      // Output number too big (last int32 filled too far)
      return false;
  }

  j = 0;
  if (bytesleft) {
    for (i = bytesleft; i > 0; --i) {
      *(binu++) = (outi[0] >> (8 * (i - 1))) & 0xff;
    }
    ++j;
  }

  for (; j < outisz; ++j) {
    for (i = sizeof(*outi); i > 0; --i) {
      *(binu++) = (outi[j] >> (8 * (i - 1))) & 0xff;
    }
  }

  // Count canonical base58 byte count
  binu = bin;
  for (i = 0; i < binsz; ++i) {
    if (binu[i]) {
      if (zerocount > i) {
        /* result too large */
        return false;
      }

      break;
    }
    --*binszp;
  }
  *binszp += zerocount;

  return true;
}

int ripple_b58check(const void *bin, size_t binsz, HasherType hasher_type,
                    const char *base58str) {
  unsigned char buf[32] = {0};
  const uint8_t *binc = bin;
  unsigned i = 0;
  if (binsz < 4) return -4;
  hasher_Raw(hasher_type, bin, binsz - 4, buf);
  if (memcmp(&binc[binsz - 4], buf, 4)) return -1;

  // Check number of zeros is correct AFTER verifying checksum (to avoid
  // possibility of accessing base58str beyond the end)
  for (i = 0; binc[i] == '\0' && base58str[i] == 'r'; ++i) {
  }  // Just finding the end of zeros, nothing to do in loop
  if (binc[i] == '\0' || base58str[i] == 'r') return -3;

  return binc[0];
}

bool ripple_b58enc(char *b58, size_t *b58sz, const void *data, size_t binsz) {
  const uint8_t *bin = data;
  int carry = 0;
  size_t i = 0, j = 0, high = 0, zcount = 0;
  size_t size = 0;

  while (zcount < binsz && !bin[zcount]) ++zcount;

  size = (binsz - zcount) * 138 / 100 + 1;
  uint8_t buf[size];
  memzero(buf, size);

  for (i = zcount, high = size - 1; i < binsz; ++i, high = j) {
    for (carry = bin[i], j = size - 1; (j > high) || carry; --j) {
      carry += 256 * buf[j];
      buf[j] = carry % 58;
      carry /= 58;
      if (!j) {
        // Otherwise j wraps to maxint which is > high
        break;
      }
    }
  }

  for (j = 0; j < size && !buf[j]; ++j)
    ;

  if (*b58sz <= zcount + size - j) {
    *b58sz = zcount + size - j + 1;
    return false;
  }

  if (zcount) memset(b58, 'r', zcount);
  for (i = zcount; j < size; ++i, ++j)
    b58[i] = ripple_b58digits_ordered[buf[j]];
  b58[i] = '\0';
  *b58sz = i + 1;

  return true;
}

int ripple_encode_check(const uint8_t *data, int datalen,
                        HasherType hasher_type, char *str, int strsize) {
  if (datalen > 128) {
    return 0;
  }
  uint8_t buf[datalen + 32];
  memset(buf, 0, sizeof(buf));
  uint8_t *hash = buf + datalen;
  memcpy(buf, data, datalen);
  hasher_Raw(hasher_type, data, datalen, hash);
  size_t res = strsize;
  bool success = ripple_b58enc(str, &res, buf, datalen + 4);
  memzero(buf, sizeof(buf));
  return success ? res : 0;
}

int ripple_decode_check(const char *str, HasherType hasher_type, uint8_t *data,
                        int datalen) {
  if (datalen > 128) {
    return 0;
  }
  uint8_t d[datalen + 4];
  memset(d, 0, sizeof(d));
  size_t res = datalen + 4;
  if (ripple_b58tobin(d, &res, str) != true) {
    return 0;
  }
  uint8_t *nd = d + datalen + 4 - res;
  if (ripple_b58check(nd, res, hasher_type, str) < 0) {
    return 0;
  }
  memcpy(data, nd, res - 4);
  return res - 4;
}
