//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_DECODER_H_
#define HDLC_DECODER_H_

#include <stddef.h>
#include <stdint.h>
#include "hdlc_framing.h"

typedef enum {
    HDLC_DECODER_FRAME_FLAG_SIG,
    HDLC_DECODER_ESCAPE_SIG,
    HDLC_DECODER_BYTE_SIG,
} HdlcDecoderSignal;

typedef struct {
    HdlcDecoderSignal sig;
    uint8_t byte;
} HdlcDecoderEvt;

typedef struct {
    uint8_t *raw;
    size_t size;
} HdlcDecoderFrame;

typedef struct {
    HdlcDecoderFrame *(*acquire)(void *ctx);
    void *ctx;
} HdlcDecoderAcquireSink;

typedef struct {
    void (*emit)(void *ctx, HdlcDecoderFrame *frame);
    void *ctx;
} HdlcDecoderFrameSink;

typedef struct {
    HdlcDecoderFrame *frame;
    size_t frameSize;
    uint8_t payloadRemaining;
    uint8_t checksum;
    uint8_t state;
    HdlcFramingVtbl const *framing;
    HdlcDecoderAcquireSink acquire;
    HdlcDecoderFrameSink sink;
} HdlcDecoder;

void HdlcDecoder_ctor(HdlcDecoder *me,
                      HdlcFramingVtbl const *framing,
                      HdlcDecoderAcquireSink acquire,
                      HdlcDecoderFrameSink sink);
void HdlcDecoder_dispatch(HdlcDecoder *me, HdlcDecoderEvt const *e);

#endif
