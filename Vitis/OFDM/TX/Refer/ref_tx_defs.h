#ifndef REF_TX_DEFS_H
#define REF_TX_DEFS_H

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>
#include "hls_half.h"
#include <stdint.h> // 新增
#include <string.h> // 新增 (為了安全的 memcpy)

// ---------------------------------------------------------
// 資料介面定義 (對齊 Packed 32-bit 規範)
// ---------------------------------------------------------
// [31:16] = Q (Imag), [15:0] = I (Real)
typedef ap_axiu<32, 0, 0, 0> axis_t;

// ---------------------------------------------------------
// 系統參數 (2048-FFT OFDM)
// ---------------------------------------------------------
#define FFT_LENGTH 2048                         // 總頻點數
#define DATA_TONES 1632                         // 數據頻點 (1905 - 273)
#define PILOT_TONES 273                         // 導頻頻點 (1905 / 7)
#define LEFT_GUARD 72                           // 左側保護頻帶
#define RIGHT_GUARD 71                          // 右側保護頻帶
#define ACTIVE_TONES (DATA_TONES + PILOT_TONES) // 1905 個有效頻點

// ---------------------------------------------------------
// Helper Function: 32-bit 複雜信號封裝
// ---------------------------------------------------------
// 確保格式與 Mapper/Demapper 的 [Q, I] 順序一致
static inline ap_uint<32> pack_complex(half i, half q)
{
    unsigned short us_i = *(unsigned short *)&i;
    unsigned short us_q = *(unsigned short *)&q;
    ap_uint<32> val;
    val(15, 0) = us_i;  // 低 16 位放 I
    val(31, 16) = us_q; // 高 16 位放 Q
    return val;
}
static inline void unpack_complex(ap_uint<32> val, half &i, half &q)
{
    uint16_t us_i = val(15, 0);
    uint16_t us_q = val(31, 16);
    memcpy(&i, &us_i, 2);
    memcpy(&q, &us_q, 2);
}
static inline ap_uint<32> pack_int16(int16_t i, int16_t q)
{
    ap_uint<32> val;
    val(15, 0) = (uint16_t)i;
    val(31, 16) = (uint16_t)q;
    return val;
}

// ---------------------------------------------------------
// Pilot ROM (與 Planar 版本共用，導頻為純實數序列)
// ---------------------------------------------------------
static const half PILOT_ROM[273] = {
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f,
    (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f,
    (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)1.33333f,
    (half)-1.33333f, (half)1.33333f, (half)1.33333f, (half)1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f, (half)-1.33333f,
    (half)1.33333f};

#endif