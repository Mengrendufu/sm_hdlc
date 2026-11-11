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
#include "hdlc_encoder.h"
#include "hdlc_test_frame.h"
#include "hdlc_test_sink.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

static uint8_t zeroFlag_(void) {
    return 0U;
}

static uint8_t oneEscape_(void) {
    return 1U;
}

static uint8_t transform_(uint8_t byte) {
    return byte ^ 0x80U;
}

static HdlcFramingVtbl const zeroFraming_ = {
    &zeroFlag_, &oneEscape_, &transform_,
};

static size_t reference_(uint8_t *wire,
                          HdlcFramingVtbl const *framing,
                          uint8_t sequence,
                          uint8_t recId,
                          uint8_t const *payload,
                          size_t size)
{
    uint8_t frame[4U + UINT8_MAX];
    size_t count = HdlcTest_buildFrame_(
        frame, sequence, recId, payload, size);
    size_t n = 0U;
    wire[n++] = (*framing->frameFlag)();
    for (size_t i = 0U; i < count; ++i) {
        uint8_t byte = frame[i];
        if (byte == (*framing->frameFlag)()
            || byte == (*framing->escape)())
        {
            wire[n++] = (*framing->escape)();
            byte = (*framing->transform)(byte);
        }
        wire[n++] = byte;
    }
    wire[n++] = (*framing->frameFlag)();
    return n;
}

int main(void) {
    HdlcEncoder encoder;
    HdlcTestByteSink output = {0};
    HdlcFramingVtbl const *const profiles[] = {
        &HdlcFraming_defaultVtbl, &zeroFraming_,
    };
    uint8_t payload[UINT8_MAX];
    uint8_t raw[4U + UINT8_MAX];
    HdlcEncoderFrameSink sink = {
        raw, {&HdlcTest_captureByte_, &output}, {&HdlcTest_gc_, NULL},
    };
    uint8_t expected[sizeof(output.wire)];
    for (size_t profile = 0U; profile < 2U; ++profile) {
        HdlcFramingVtbl const *framing = profiles[profile];
        HdlcEncoder_ctor(&encoder, framing, (HdlcEncoderNotifySink){0});
        CHECK(encoder.sink == NULL);
        for (unsigned size = 0U; size <= UINT8_MAX; ++size) {
            for (unsigned seed = 0U; seed <= UINT8_MAX; ++seed) {
                for (size_t i = 0U; i < size; ++i) {
                    payload[i] = (uint8_t)(seed + i + size);
                }
                uint8_t recId = (uint8_t)(seed ^ 0xA5U);
                size_t count = reference_(expected, framing,
                    (uint8_t)seed, recId, payload, size);
                output.size = 0U;
                HdlcTest_buildFrame_(raw, (uint8_t)seed,
                                     recId, payload, size);
                HdlcEncoder_init(&encoder, &sink);
                CHECK(output.size == 0U);
                CHECK(encoder.sink == &sink && encoder.idx == 0U);
                for (size_t i = 0U; i < count; ++i) {
                    CHECK(encoder.sink == &sink);
                    CHECK(output.size == i);
                    HdlcEncoder_dispatch(&encoder);
                    CHECK(output.size == i + 1U);
                    CHECK(output.wire[i] == expected[i]);
                }
                CHECK(encoder.sink == &sink);
                HdlcEncoder_dispatch(&encoder);
                CHECK(encoder.sink == NULL);
                HdlcEncoder_dispatch(&encoder);
                CHECK(output.size == count);
            }
        }
    }

    HdlcEncoder_ctor(&encoder, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){0});
    memset(payload, 0x7E, sizeof(payload));
    size_t count = reference_(expected, encoder.framing,
                              0x7DU, 0x7EU, payload, 0x7DU);
    for (size_t cut = 0U; cut <= count; ++cut) {
        output.size = 0U;
        HdlcTest_buildFrame_(raw, 0x7DU, 0x7EU, payload, 0x7DU);
        HdlcEncoder_init(&encoder, &sink);
        for (size_t i = 0U; i < cut; ++i) {
            HdlcEncoder_dispatch(&encoder);
        }
        CHECK(encoder.sink == &sink && output.size == cut);
        CHECK(encoder.framing == &HdlcFraming_defaultVtbl);
        CHECK(encoder.notify.notify == NULL);
        for (size_t i = cut; i < count; ++i) {
            HdlcEncoder_dispatch(&encoder);
        }
        CHECK(encoder.sink == &sink);
        HdlcEncoder_dispatch(&encoder);
        CHECK(encoder.sink == NULL);
        HdlcEncoder_dispatch(&encoder);
        CHECK(output.size == count);
        CHECK(memcmp(output.wire, expected, count) == 0);
    }
    return 0;
}
