//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_TEST_SINK_H_
#define HDLC_TEST_SINK_H_

#include <stdbool.h>
#include <stdlib.h>
#include "hdlc_encoder.h"
#include "hdlc_test_frame.h"

typedef struct {
    uint8_t wire[2U + 2U * (4U + UINT8_MAX)];
    size_t size;
    bool ready;
} HdlcTestByteSink;

static inline void HdlcTest_captureByte_(void *ctx, uint8_t byte) {
    HdlcTestByteSink *sink = ctx;
    if (sink->size >= sizeof(sink->wire)) {
        abort();
    }
    sink->wire[sink->size++] = byte;
}

static inline void HdlcTest_gc_(void *ctx, uint8_t const *raw) {
    (void)ctx;
    (void)raw;
}

static inline void HdlcTest_notify_(void *ctx, HdlcEncoderStatus status) {
    HdlcTestByteSink *sink = ctx;
    sink->ready = status == HDLC_ENCODER_READY;
}

static inline void HdlcTest_encode_(HdlcEncoder *encoder,
                                    HdlcTestByteSink *output,
                                    uint8_t seq,
                                    uint8_t recId,
                                    uint8_t const *payload,
                                    size_t size)
{
    if (size > UINT8_MAX) {
        abort();
    }
    uint8_t raw[4U + UINT8_MAX];
    HdlcTest_buildFrame_(raw, seq, recId, payload, size);
    HdlcEncoderFrameSink sink = {
        raw, {&HdlcTest_captureByte_, output}, {&HdlcTest_gc_, NULL},
    };
    HdlcEncoder_init(encoder, &sink);
    while (!output->ready) {
        HdlcEncoder_dispatch(encoder);
    }
}

#endif
