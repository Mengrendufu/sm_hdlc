//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdio.h>
#include <string.h>
#include "hdlc_decoder.h"
#include "hdlc_encoder.h"
#include "hdlc_test_router.h"
#include "hdlc_test_sink.h"
#include "hdlc_test_frame.h"
#include "hdlc_test_carrier.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

static uint8_t frameFlag_(void) {
    return 0xA5U;
}

static uint8_t escape_(void) {
    return 0xA4U;
}

static uint8_t encode_(uint8_t byte) {
    return (uint8_t)(byte + 0x10U);
}

static uint8_t decode_(uint8_t byte) {
    return (uint8_t)(byte - 0x10U);
}

static HdlcFramingVtbl const customTx_ = {
    &frameFlag_, &escape_, &encode_,
};

static HdlcFramingVtbl const customRx_ = {
    &frameFlag_, &escape_, &decode_,
};

typedef struct {
    uint8_t frame[3U + UINT8_MAX];
    size_t size;
    unsigned count;
} Capture;

static void capture_(void *ctx, HdlcDecoderFrame *frame) {
    Capture *capture = ctx;
    size_t size = 3U + frame->raw[2];
    memcpy(capture->frame, frame->raw, size);
    capture->size = size;
    ++capture->count;
}

int main(void) {
    HdlcEncoder a, b;
    HdlcDecoder rxA, rxB;
    Capture ca = {0}, cb = {0};
    HdlcTestFrameSource sourceA = {0}, sourceB = {0};
    HdlcTestByteSink outA = {0}, outB = {0};
    HdlcEncoder_ctor(&a, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&HdlcTest_notify_, &outA});
    HdlcEncoder_ctor(&b, &customTx_,
                     (HdlcEncoderNotifySink){&HdlcTest_notify_, &outB});
    HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                     (HdlcDecoderFrameSink){&capture_, &ca});
    HdlcDecoder_ctor(&rxB, &customRx_,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceB},
                     (HdlcDecoderFrameSink){&capture_, &cb});
    uint8_t *wa = outA.wire;
    uint8_t *wb = outB.wire;
    uint8_t const special[] = {
        0xA5U, 0xA4U, 0x7EU, 0x7DU,
    };
    uint8_t const expected[] = {
        0xA5U, 0U, 0xA4U, 0xB5U, 4U,
        0xA4U, 0xB5U, 0xA4U, 0xB4U, 0x7EU, 0x7DU, 0x12U, 0xA5U,
    };
    HdlcTest_encode_(&b, &outB,
                     0U, 0xA5U, special, sizeof(special));
    CHECK(outB.size == sizeof(expected));
    CHECK(memcmp(wb, expected, sizeof(expected)) == 0);
    HdlcEncoder_ctor(&b, &customTx_,
                     (HdlcEncoderNotifySink){&HdlcTest_notify_, &outB});

    uint8_t payload[UINT8_MAX];
    uint8_t frame[4U + UINT8_MAX];
    HdlcEncoderFrameSink sinkA = {
        frame, {&HdlcTest_captureByte_, &outA}, {&HdlcTest_gc_, NULL},
    };
    HdlcEncoderFrameSink sinkB = {
        frame, {&HdlcTest_captureByte_, &outB}, {&HdlcTest_gc_, NULL},
    };
    for (unsigned length = 0U; length <= UINT8_MAX; ++length) {
        for (unsigned seed = 0U; seed <= UINT8_MAX; ++seed) {
            for (size_t i = 0U; i < length; ++i) {
                payload[i] = (uint8_t)(seed + i);
            }
            uint8_t seq = (uint8_t)seed;
            size_t size = HdlcTest_buildFrame_(
                frame, seq, (uint8_t)seed, payload, length);
            outA.size = 0U;
            outB.size = 0U;
            HdlcEncoder_init(&a, &sinkA);
            HdlcEncoder_init(&b, &sinkB);
            CHECK(outA.size == 0U && outB.size == 0U);
            while (!outA.ready || !outB.ready) {
                size_t beforeA = outA.size;
                size_t beforeB = outB.size;
                HdlcEncoder_dispatch(&a);
                CHECK(outA.size - beforeA <= 1U);
                CHECK(outB.size == beforeB);
                HdlcEncoder_dispatch(&b);
                CHECK(outB.size - beforeB <= 1U);
            }
            size_t sa = outA.size;
            size_t sb = outB.size;
            CHECK(sa > 0U && sb > 0U);
            CHECK(wb[0] == 0xA5U && wb[sb - 1U] == 0xA5U);
            for (size_t i = 0U; i < sa || i < sb; ++i) {
                if (i < sa) {
                    HdlcTest_input_(&rxA, wa[i]);
                }
                if (i < sb) {
                    HdlcTest_input_(&rxB, wb[i]);
                }
            }
            unsigned count = length * 256U + seed + 1U;
            CHECK(ca.count == count && cb.count == count);
            CHECK(ca.size == length + 3U && cb.size == ca.size);
            CHECK(memcmp(ca.frame, cb.frame, ca.size) == 0);
            CHECK(cb.size == size - 1U);
            CHECK(memcmp(cb.frame, frame, cb.size) == 0);
            CHECK(cb.frame[0] == seq && cb.frame[1] == (uint8_t)seed);
            CHECK(cb.frame[2] == length);
            CHECK(memcmp(&cb.frame[3], payload, length) == 0);
        }
    }

    uint8_t const resync[] = {
        0xA5U, 1U, 0xA4U, 0xA5U, 0U, 0U, 0U, 0xFFU,
    };
    unsigned count = cb.count;
    for (size_t i = 0U; i < sizeof(resync); ++i) {
        HdlcTest_input_(&rxB, resync[i]);
    }
    CHECK(cb.count == count + 1U && cb.size == 3U);
    uint8_t const badEscape[] = {0xA5U, 0xA4U, 0xA4U, 0U, 0U, 0U, 0xFFU};
    for (size_t i = 0U; i < sizeof(badEscape); ++i) {
        HdlcTest_input_(&rxB, badEscape[i]);
    }
    CHECK(cb.count == count + 1U);
    HdlcDecoder_ctor(&rxB, &customRx_,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceB},
                     (HdlcDecoderFrameSink){&capture_, &cb});
    CHECK(rxB.framing == &customRx_);
    CHECK(ca.count == 65536U);
    return 0;
}
