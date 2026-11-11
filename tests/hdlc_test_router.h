//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_TEST_ROUTER_H_
#define HDLC_TEST_ROUTER_H_

#include "hdlc_decoder.h"

static void HdlcTest_input_(HdlcDecoder *me, uint8_t byte) {
    HdlcDecoderEvt const e = {
        .sig = byte == (*me->framing->frameFlag)()
               ? HDLC_DECODER_FRAME_FLAG_SIG
               : (byte == (*me->framing->escape)()
                  ? HDLC_DECODER_ESCAPE_SIG : HDLC_DECODER_BYTE_SIG),
        .byte = byte,
    };
    HdlcDecoder_dispatch(me, &e);
}

#endif
