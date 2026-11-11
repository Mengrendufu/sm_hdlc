//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "hdlc_decoder.h"
#include "hdlc_encoder.h"
#include "hdlc_test_port.h"
#include "hdlc_test_carrier.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

#define EXPECT_FAULT(action_) do { \
    expectFault_ = true; \
    if (setjmp(fault_) == 0) { \
        action_; \
        CHECK(false); \
    } \
    expectFault_ = false; \
} while (0)

static jmp_buf fault_;
static bool expectFault_;
static unsigned checks_;
static unsigned frames_;
static unsigned bytes_;
static unsigned reclaimed_;

void HdlcTest_assert(int condition) {
    ++checks_;
    if (!condition) {
        if (expectFault_) {
            longjmp(fault_, 1);
        }
        abort();
    }
}

static void capture_(void *ctx, HdlcDecoderFrame *frame) {
    (void)ctx;
    (void)frame;
    ++frames_;
}

static void captureByte_(void *ctx, uint8_t byte) {
    (void)ctx;
    (void)byte;
    ++bytes_;
}

static void gc_(void *ctx, uint8_t const *raw) {
    (void)ctx;
    (void)raw;
    ++reclaimed_;
}

static uint8_t identity_(uint8_t byte) {
    return byte;
}

static HdlcDecoderFrame *provide_(void *ctx) {
    return ctx;
}

int main(void) {
    HdlcDecoder rx;
    HdlcTestFrameSource source = {0};
    HdlcDecoderFrame provided = {NULL, 3U};
    HdlcDecoderAcquireSink const sourceSink = {
        &HdlcTest_acquireFrame_, &source,
    };
    HdlcDecoderAcquireSink const providedSink = {&provide_, &provided};
    HdlcDecoderAcquireSink const noAcquire = {NULL, NULL};
    HdlcDecoderAcquireSink const nullFrame = {&provide_, NULL};
    HdlcDecoderFrameSink const captureSink = {&capture_, NULL};
    HdlcDecoderFrameSink const noEmit = {NULL, NULL};
    HdlcEncoder tx;
    HdlcEncoderNotifySink const noNotify = {0};
    uint8_t raw[] = {0U, 0U, 0U};
    HdlcEncoderFrameSink sink = {
        raw, {&captureByte_, NULL}, {&gc_, NULL},
    };
    HdlcDecoderEvt const flag = {HDLC_DECODER_FRAME_FLAG_SIG, 0U};
    HdlcDecoderEvt const invalid = {(HdlcDecoderSignal)99, 0U};
    HdlcDecoderEvt const frame[] = {
        {HDLC_DECODER_FRAME_FLAG_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0U},
        {HDLC_DECODER_BYTE_SIG, 0xFFU},
    };
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     sourceSink, captureSink);
    CHECK(checks_ > 0U);
    unsigned before = checks_;
    HdlcEncoder_ctor(&tx, &HdlcFraming_defaultVtbl, noNotify);
    CHECK(checks_ > before);
    before = checks_;
    HdlcEncoder_init(&tx, &sink);
    for (unsigned i = 0U; i < 6U; ++i) {
        HdlcEncoder_dispatch(&tx);
    }
    CHECK(bytes_ == 6U && reclaimed_ == 0U);
    EXPECT_FAULT(HdlcEncoder_init(&tx, &sink));
    HdlcEncoder_dispatch(&tx);
    CHECK(bytes_ == 6U && reclaimed_ == 1U);
    CHECK(checks_ > before);
    for (size_t i = 0U; i < sizeof(frame) / sizeof(frame[0]); ++i) {
        HdlcDecoder_dispatch(&rx, &frame[i]);
    }
    CHECK(frames_ == 1U);

    EXPECT_FAULT(HdlcDecoder_ctor(NULL, &HdlcFraming_defaultVtbl,
                                   sourceSink, captureSink));
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                                   sourceSink, noEmit));
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                                   noAcquire, captureSink));
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, NULL,
                                   sourceSink, captureSink));
    EXPECT_FAULT(HdlcDecoder_dispatch(NULL, &flag));
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, NULL));
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &invalid));
    EXPECT_FAULT(HdlcEncoder_ctor(NULL, &HdlcFraming_defaultVtbl,
                                 noNotify));
    EXPECT_FAULT(HdlcEncoder_ctor(&tx, NULL, noNotify));
    CHECK(bytes_ == 6U && tx.sink == NULL);
    EXPECT_FAULT(HdlcEncoder_init(NULL, &sink));
    EXPECT_FAULT(HdlcEncoder_init(&tx, NULL));
    HdlcEncoderFrameSink invalidSink = sink;
    invalidSink.raw = NULL;
    EXPECT_FAULT(HdlcEncoder_init(&tx, &invalidSink));
    invalidSink = sink;
    invalidSink.emit.emit = NULL;
    EXPECT_FAULT(HdlcEncoder_init(&tx, &invalidSink));
    invalidSink = sink;
    invalidSink.gc.gc = NULL;
    EXPECT_FAULT(HdlcEncoder_init(&tx, &invalidSink));
    CHECK(reclaimed_ == 1U && tx.sink == NULL);
    EXPECT_FAULT(HdlcEncoder_dispatch(NULL));
    HdlcEncoder_dispatch(&tx);
    CHECK(bytes_ == 6U);
    HdlcEncoder_init(&tx, &sink);
    EXPECT_FAULT(HdlcEncoder_init(&tx, &sink));
    CHECK(bytes_ == 6U && tx.sink == &sink);
    for (unsigned i = 0U; i < 7U; ++i) {
        HdlcEncoder_dispatch(&tx);
    }
    CHECK(bytes_ == 12U && reclaimed_ == 2U && tx.sink == NULL);

    HdlcEncoder_init(&tx, &sink);
    HdlcEncoder_dispatch(&tx);
    tx.idx = 3U;
    EXPECT_FAULT(HdlcEncoder_dispatch(&tx));
    tx.idx = 0U;
    for (unsigned i = 0U; i < 6U; ++i) {
        HdlcEncoder_dispatch(&tx);
    }
    CHECK(bytes_ == 18U && reclaimed_ == 3U && tx.sink == NULL);
    tx.state = UINT8_MAX;
    EXPECT_FAULT(HdlcEncoder_dispatch(&tx));
    HdlcEncoder_ctor(&tx, &HdlcFraming_defaultVtbl, noNotify);

    HdlcFramingVtbl incomplete = HdlcFraming_defaultVtbl;
    incomplete.frameFlag = NULL;
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, &incomplete,
                                   sourceSink, captureSink));
    EXPECT_FAULT(HdlcEncoder_ctor(&tx, &incomplete, noNotify));
    HdlcDecoderEvt const zero = {HDLC_DECODER_BYTE_SIG, 0U};
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     nullFrame, captureSink);
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &flag));
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     providedSink, captureSink);
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &flag));
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     providedSink, captureSink);
    provided = (HdlcDecoderFrame){source.raw, 2U};
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &flag));
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     providedSink, captureSink);
    provided.size = sizeof(source.raw);
    HdlcDecoder_dispatch(&rx, &flag);
    rx.frameSize = provided.size;
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &zero));
    rx.frameSize = 0U;
    HdlcDecoder_dispatch(&rx, &zero);
    rx.frameSize = provided.size;
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &zero));
    rx.frameSize = 1U;
    HdlcDecoder_dispatch(&rx, &zero);
    HdlcDecoderEvt const one = {HDLC_DECODER_BYTE_SIG, 1U};
    HdlcDecoder_dispatch(&rx, &one);
    rx.frameSize = provided.size;
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &zero));
    rx.frameSize = 3U;
    HdlcDecoderEvt const escape = {HDLC_DECODER_ESCAPE_SIG, 0U};
    HdlcDecoder_dispatch(&rx, &escape);
    rx.frameSize = provided.size;
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &zero));
    rx.frameSize = 3U;
    HdlcDecoderEvt const escapedZero = {HDLC_DECODER_BYTE_SIG, 0x20U};
    HdlcDecoder_dispatch(&rx, &escapedZero);
    HdlcDecoderEvt const checksum = {HDLC_DECODER_BYTE_SIG, 0xFEU};
    HdlcDecoder_dispatch(&rx, &checksum);
    CHECK(rx.frame == NULL && frames_ == 2U);
    rx.state = UINT8_MAX;
    EXPECT_FAULT(HdlcDecoder_dispatch(&rx, &flag));
    HdlcDecoder_ctor(&rx, &HdlcFraming_defaultVtbl,
                     sourceSink, captureSink);

    incomplete = HdlcFraming_defaultVtbl;
    incomplete.escape = NULL;
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, &incomplete,
                                   sourceSink, captureSink));
    EXPECT_FAULT(HdlcEncoder_ctor(&tx, &incomplete, noNotify));
    incomplete = HdlcFraming_defaultVtbl;
    incomplete.transform = NULL;
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, &incomplete,
                                   sourceSink, captureSink));
    EXPECT_FAULT(HdlcEncoder_ctor(&tx, &incomplete, noNotify));
    incomplete = HdlcFraming_defaultVtbl;
    incomplete.frameFlag = incomplete.escape;
    EXPECT_FAULT(HdlcDecoder_ctor(&rx, &incomplete,
                                   sourceSink, captureSink));
    EXPECT_FAULT(HdlcEncoder_ctor(&tx, &incomplete, noNotify));
    incomplete = HdlcFraming_defaultVtbl;
    incomplete.transform = &identity_;
    HdlcEncoder_ctor(&tx, &incomplete, noNotify);
    raw[0] = 0x7EU;
    HdlcEncoder_init(&tx, &sink);
    HdlcEncoder_dispatch(&tx);
    HdlcEncoder_dispatch(&tx);
    unsigned sent = bytes_;
    EXPECT_FAULT(HdlcEncoder_dispatch(&tx));
    CHECK(bytes_ == sent);
    raw[0] = 0x7DU;
    EXPECT_FAULT(HdlcEncoder_dispatch(&tx));
    CHECK(bytes_ == sent);

    return 0;
}
