//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_TEST_CARRIER_H_
#define HDLC_TEST_CARRIER_H_

#include "hdlc_decoder.h"

typedef struct {
    HdlcDecoderFrame frame;
    uint8_t raw[3U + UINT8_MAX];
} HdlcTestFrameSource;

static inline HdlcDecoderFrame *HdlcTest_acquireFrame_(void *ctx) {
    HdlcTestFrameSource *source = ctx;
    source->frame = (HdlcDecoderFrame){source->raw, sizeof(source->raw)};
    return &source->frame;
}

#endif
