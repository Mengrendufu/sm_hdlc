//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_ENCODER_H_
#define HDLC_ENCODER_H_

#include <stddef.h>
#include <stdint.h>
#include "hdlc_framing.h"

typedef struct {
    void (*emit)(void *ctx, uint8_t byte);
    void *ctx;
} HdlcEncoderByteSink;

typedef struct {
    void (*gc)(void *ctx, uint8_t const *raw);
    void *ctx;
} HdlcEncoderGcSink;

typedef enum {
    HDLC_ENCODER_BUSY,
    HDLC_ENCODER_READY,
} HdlcEncoderStatus;

typedef struct {
    void (*notify)(void *ctx, HdlcEncoderStatus status);
    void *ctx;
} HdlcEncoderNotifySink;

typedef struct {
    uint8_t const *raw;
    HdlcEncoderByteSink emit;
    HdlcEncoderGcSink gc;
} HdlcEncoderFrameSink;

typedef struct {
    uint8_t state;
    size_t idx;
    uint8_t chksum;
    HdlcEncoderFrameSink const *sink;
    HdlcFramingVtbl const *framing;
    HdlcEncoderNotifySink notify;
} HdlcEncoder;

void HdlcEncoder_ctor(HdlcEncoder *me,
                      HdlcFramingVtbl const *framing,
                      HdlcEncoderNotifySink notify);
void HdlcEncoder_init(HdlcEncoder *me, HdlcEncoderFrameSink const *sink);
void HdlcEncoder_dispatch(HdlcEncoder *me);

#endif
