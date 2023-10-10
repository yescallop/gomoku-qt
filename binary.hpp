#pragma once

#include "common.hpp"

u32 zigzag_encode(i32 x) { return (x << 1) ^ (x >> 31); }
i32 zigzag_decode(u32 x) { return (x >> 1) ^ -(x & 1); }

u32 interleave(u32 x, u32 y) {
    auto scatter = [](u32 x) {
        x = (x | (x << 8)) & 0x00ff00ff;
        x = (x | (x << 4)) & 0x0f0f0f0f;
        x = (x | (x << 2)) & 0x33333333;
        return (x | (x << 1)) & 0x55555555;
    };
    return scatter(x) | (scatter(y) << 1);
}

pair<u32, u32> deinterleave(u32 i) {
    auto gather = [](u32 x) {
        x &= 0x55555555;
        x = (x | (x >> 1)) & 0x33333333;
        x = (x | (x >> 2)) & 0x0f0f0f0f;
        x = (x | (x >> 4)) & 0x00ff00ff;
        return (x | (x >> 8)) & 0x0000ffff;
    };
    return {gather(i), gather(i >> 1)};
}

void write_var_u14(QByteArray &buf, u32 val) {
    if (val & 0x3f80) {
        buf.append((val & 0x7f) | 0x80);
        val >>= 7;
    }
    buf.append(val & 0x7f);
}

optional<u32> read_var_u14(const QByteArray &buf, usize &read) {
    if (read >= buf.size())
        return nullopt;

    char lo = buf[read++], hi = 0;
    if (lo & 0x80) {
        if (read >= buf.size())
            return nullopt;

        hi = buf[read++];
        if (hi & 0x80)
            return nullopt;
    }
    return (hi << 7) | (lo & 0x7f);
}
