//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "hdlc_decoder.h"
#include "hdlc_encoder.h"
#include "hdlc_test_router.h"
#include "hdlc_test_frame.h"
#include "hdlc_test_sink.h"
#include "hdlc_test_carrier.h"

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

static void routeByte_(void *ctx, uint8_t byte) {
    HdlcTest_input_(ctx, byte);
}

static void notify_(void *ctx, HdlcEncoderStatus status) {
    bool *ready = ctx;
    *ready = status == HDLC_ENCODER_READY;
}

int main(void) {
    HdlcEncoder tx;
    HdlcDecoder rx;
    Capture capture = {0};
    HdlcTestFrameSource source = {0};
    bool ready = false;
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&HdlcTest_acquireFrame_, &source},
                     (HdlcDecoderFrameSink){&capture_, &capture});
    HdlcEncoder_ctor(&tx, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&notify_, &ready});
    uint8_t payload[UINT8_MAX];
    uint8_t frame[4U + UINT8_MAX];
    uint8_t seq = 0U;
    for (unsigned length = 0U; length <= UINT8_MAX; ++length) {
        for (unsigned seed = 0U; seed <= UINT8_MAX; ++seed) {
            for (size_t i = 0U; i < length; ++i) {
                payload[i] = (uint8_t)(seed + i);
            }
            size_t size = HdlcTest_buildFrame_(
                frame, seq, (uint8_t)seed, payload, length);
            unsigned count = capture.count;
            HdlcEncoderFrameSink sink = {
                frame, {&routeByte_, &rx}, {&HdlcTest_gc_, NULL},
            };
            HdlcEncoder_init(&tx, &sink);
            while (!ready) {
                HdlcEncoder_dispatch(&tx);
            }
            if (capture.count != count + 1U
                || capture.size != length + 3U
                || capture.frame[0] != seq
                || capture.frame[1] != (uint8_t)seed
                || capture.frame[2] != length
                || memcmp(capture.frame, frame, size - 1U) != 0
                || memcmp(&capture.frame[3], payload, length) != 0)
            {
                fprintf(stderr, "length=%u seed=%u\n", length, seed);
                return 1;
            }
            ++seq;
            if (tx.sink != NULL) {
                return 1;
            }
        }
    }
    return 0;
}
