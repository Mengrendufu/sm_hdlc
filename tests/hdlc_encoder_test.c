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
#include "hdlc_test_sink.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

int main(void) {
    HdlcEncoder a, b;
    HdlcTestByteSink sinkA = {0}, sinkB = {0};
    HdlcEncoder_ctor(&a, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&HdlcTest_notify_, &sinkA});
    HdlcEncoder_ctor(&b, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&HdlcTest_notify_, &sinkB});

    uint8_t const payload[] = {0x10U, 0x20U};
    uint8_t const expected[] = {
        0x7EU, 0x00U, 0x02U, 0x02U, 0x10U, 0x20U, 0xCBU, 0x7EU,
    };
    sinkA.size = 0U;
    HdlcTest_encode_(&a, &sinkA,
                     0U, 2U, payload, sizeof(payload));
    CHECK(sinkA.size == sizeof(expected));
    CHECK(memcmp(sinkA.wire, expected, sizeof(expected)) == 0);
    CHECK(sinkB.size == 0U && b.sink == NULL);
    sinkB.size = 0U;
    HdlcTest_encode_(&b, &sinkB,
                     0U, 2U, payload, sizeof(payload));
    CHECK(sinkB.size == sizeof(expected));
    CHECK(memcmp(sinkB.wire, expected, sizeof(expected)) == 0);

    sinkA.size = 0U;
    HdlcTest_encode_(&a, &sinkA,
                     1U, 0x80U, NULL, 0U);
    uint8_t const escapedChecksum[] = {
        0x7EU, 0x01U, 0x80U, 0x00U, 0x7DU, 0x5EU, 0x7EU,
    };
    CHECK(sinkA.size == sizeof(escapedChecksum));
    CHECK(memcmp(sinkA.wire, escapedChecksum, sinkA.size) == 0);

    uint8_t const escapedPayload[] = {0x7EU, 0x7DU};
    uint8_t const escapedWire[] = {
        0x7EU, 0x02U, 0x7DU, 0x5EU, 0x02U,
        0x7DU, 0x5EU, 0x7DU, 0x5DU, 0x82U, 0x7EU,
    };
    sinkA.size = 0U;
    HdlcTest_encode_(&a, &sinkA,
                     2U, 0x7EU, escapedPayload, sizeof(escapedPayload));
    CHECK(sinkA.size == sizeof(escapedWire));
    CHECK(memcmp(sinkA.wire, escapedWire, sinkA.size) == 0);

    HdlcEncoder_ctor(&a, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&HdlcTest_notify_, &sinkA});
    for (unsigned seq = 0U; seq < 257U; ++seq) {
        sinkA.size = 0U;
        HdlcTest_encode_(&a, &sinkA,
                         (uint8_t)seq, 0U, NULL, 0U);
        uint8_t byte = (uint8_t)seq;
        if (byte == 0x7DU || byte == 0x7EU) {
            CHECK(sinkA.wire[1] == 0x7DU);
            CHECK(sinkA.wire[2] == (byte ^ 0x20U));
        } else {
            CHECK(sinkA.wire[1] == byte);
        }
        CHECK(sinkA.size >= 6U && sinkA.wire[sinkA.size - 1U] == 0x7EU);
        CHECK(sinkA.ready && sinkB.ready);
        CHECK(a.sink == NULL && b.sink == NULL);
    }
    return 0;
}
