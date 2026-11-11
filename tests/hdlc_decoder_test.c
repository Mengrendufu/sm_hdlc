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
#include "hdlc_test_router.h"
#include "hdlc_test_carrier.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

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

static void feed_(HdlcDecoder *me, uint8_t const *data, size_t size) {
    for (size_t i = 0U; i < size; ++i) {
        HdlcTest_input_(me, data[i]);
    }
}

int main(void) {
    Capture a = {0}, b = {0};
    HdlcDecoder rxA, rxB;
    HdlcTestFrameSource sourceA = {0}, sourceB = {0};
    HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                     (HdlcDecoderFrameSink){&capture_, &a});
    HdlcDecoder_ctor(&rxB, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceB},
                     (HdlcDecoderFrameSink){&capture_, &b});

    uint8_t const plain[] = {
        0x7EU, 0x01U, 0x02U, 0x02U, 0x10U, 0x20U, 0xCAU,
    };
    uint8_t const escaped[] = {
        0x7EU, 0x03U, 0x04U, 0x02U,
        0x7DU, 0x5EU, 0x7DU, 0x5DU, 0xFBU,
    };
    uint8_t const expected[] = {0x03U, 0x04U, 0x02U, 0x7EU, 0x7DU};
    for (size_t i = 0U; i < sizeof(escaped); ++i) {
        if (i < sizeof(plain)) {
            HdlcTest_input_(&rxA, plain[i]);
        }
        HdlcTest_input_(&rxB, escaped[i]);
    }
    CHECK(a.count == 1U && b.count == 1U);
    CHECK(a.size == 5U && memcmp(a.frame, &plain[1], 5U) == 0);
    CHECK(b.size == sizeof(expected));
    CHECK(memcmp(b.frame, expected, sizeof(expected)) == 0);

    uint8_t const bad[] = {0x7EU, 0x05U, 0x06U, 0x00U, 0x00U};
    feed_(&rxA, bad, sizeof(bad));
    CHECK(a.count == 1U && b.count == 1U);
    uint8_t const resync[] = {
        0x7EU, 0x11U, 0x22U,
        0x7EU, 0x07U, 0x08U, 0x00U, 0xF0U,
    };
    feed_(&rxA, resync, sizeof(resync));
    CHECK(a.count == 2U && a.size == 3U);
    CHECK(a.frame[0] == 0x07U && a.frame[1] == 0x08U);

    uint8_t const escapedChecksum[] = {
        0x7EU, 0x01U, 0x80U, 0x00U, 0x7DU, 0x5EU,
    };
    feed_(&rxA, escapedChecksum, sizeof(escapedChecksum));
    CHECK(a.count == 3U && a.frame[1] == 0x80U);

    feed_(&rxA, plain, 4U);
    HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                     (HdlcDecoderFrameSink){&capture_, &a});
    feed_(&rxA, &plain[4], sizeof(plain) - 4U);
    CHECK(a.count == 3U);
    feed_(&rxB, plain, 3U);
    HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                     (HdlcDecoderFrameSink){&capture_, &a});
    feed_(&rxB, &plain[3], sizeof(plain) - 3U);
    CHECK(b.count == 2U);

    for (size_t field = 0U; field < 6U; ++field) {
        unsigned count = a.count;
        HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                         (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                         (HdlcDecoderFrameSink){&capture_, &a});
        feed_(&rxA, plain, field + 1U);
        HdlcTest_input_(&rxA, 0x7DU);
        HdlcTest_input_(&rxA, 0x7DU);
        feed_(&rxA, &plain[1], sizeof(plain) - 1U);
        CHECK(a.count == count);
        feed_(&rxA, plain, sizeof(plain));
        CHECK(a.count == count + 1U);

        feed_(&rxA, plain, field + 1U);
        HdlcTest_input_(&rxA, 0x7DU);
        feed_(&rxA, plain, sizeof(plain));
        CHECK(a.count == count + 2U);
    }

    HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                     (HdlcDecoderFrameSink){&capture_, &a});
    unsigned count = a.count;
    uint8_t const escapedHeader[] = {
        0x7EU, 0x7DU, 0x5EU, 0x7DU, 0x5DU, 0x00U, 0x04U,
    };
    feed_(&rxA, escapedHeader, sizeof(escapedHeader));
    CHECK(a.count == count + 1U && a.frame[0] == 0x7EU);
    CHECK(a.frame[1] == 0x7DU && a.frame[2] == 0U);

    feed_(&rxA, &plain[1], sizeof(plain) - 1U);
    CHECK(a.count == count + 1U);
    feed_(&rxA, plain, sizeof(plain));
    CHECK(a.count == count + 2U);

    HdlcDecoder_ctor(&rxA, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &sourceA},
                     (HdlcDecoderFrameSink){&capture_, &a});
    HdlcDecoderEvt const events[] = {
        {HDLC_DECODER_FRAME_FLAG_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0x7EU},
        {HDLC_DECODER_BYTE_SIG, 0x7DU},
        {HDLC_DECODER_BYTE_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0x04U},
    };
    for (size_t i = 0U; i < sizeof(events) / sizeof(events[0]); ++i) {
        HdlcDecoder_dispatch(&rxA, &events[i]);
    }
    CHECK(a.count == count + 3U);
    CHECK(a.size == 3U && a.frame[0] == 0x7EU && a.frame[1] == 0x7DU);

    HdlcDecoderEvt const escapedEvents[] = {
        {HDLC_DECODER_FRAME_FLAG_SIG, 0x7DU},
        {HDLC_DECODER_ESCAPE_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0x5EU},
        {HDLC_DECODER_ESCAPE_SIG, 0x7EU},
        {HDLC_DECODER_BYTE_SIG, 0x5DU},
        {HDLC_DECODER_BYTE_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0x04U},
    };
    for (size_t i = 0U;
         i < sizeof(escapedEvents) / sizeof(escapedEvents[0]); ++i)
    {
        HdlcDecoder_dispatch(&rxA, &escapedEvents[i]);
    }
    CHECK(a.count == count + 4U);
    CHECK(a.size == 3U && a.frame[0] == 0x7EU && a.frame[1] == 0x7DU);
    return 0;
}
