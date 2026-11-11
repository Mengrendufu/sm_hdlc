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
#include "hdlc_port.h"
#include "hdlc_encoder.h"

enum HdlcEncoderState_ {
    HDLC_TX_IDLE_,
    HDLC_TX_SEQ_,
    HDLC_TX_SEQ_ESCAPED_,
    HDLC_TX_REC_ID_,
    HDLC_TX_REC_ID_ESCAPED_,
    HDLC_TX_LENGTH_,
    HDLC_TX_LENGTH_ESCAPED_,
    HDLC_TX_PAYLOAD_,
    HDLC_TX_PAYLOAD_ESCAPED_,
    HDLC_TX_CHECKSUM_,
    HDLC_TX_CHECKSUM_ESCAPED_,
    HDLC_TX_CLOSE_FLAG_,
    HDLC_TX_INACTIVE_,
    HDLC_TX_DEAD_,
};

static void HdlcEncoder_emit_(HdlcEncoder *me, uint8_t byte) {
    (*me->sink->emit.emit)(me->sink->emit.ctx, byte);
}

static uint8_t HdlcEncoder_current_(HdlcEncoder const *me) {
    HDLC_ASSERT(me->idx < 3U + me->sink->raw[2]);
    return me->sink->raw[me->idx];
}

static bool HdlcEncoder_payloadDone_(HdlcEncoder const *me) {
    return me->idx == 3U + me->sink->raw[2];
}

static void HdlcEncoder_notify_(HdlcEncoder *me, HdlcEncoderStatus status) {
    if (me->notify.notify != NULL) {
        (*me->notify.notify)(me->notify.ctx, status);
    }
}

static bool HdlcEncoder_needsEscape_(HdlcEncoder const *me, uint8_t byte) {
    return byte == (*me->framing->escape)()
           || byte == (*me->framing->frameFlag)();
}

static uint8_t HdlcEncoder_escape_(HdlcEncoder const *me, uint8_t byte) {
    uint8_t const encoded = (*me->framing->transform)(byte);
    HDLC_ASSERT(encoded != (*me->framing->frameFlag)()
                && encoded != (*me->framing->escape)());
    return encoded;
}

void HdlcEncoder_ctor(HdlcEncoder *me,
                      HdlcFramingVtbl const *framing,
                      HdlcEncoderNotifySink notify)
{
    HDLC_ASSERT(me != NULL);
    HDLC_ASSERT(framing != NULL);
    HDLC_ASSERT(framing->frameFlag != NULL);
    HDLC_ASSERT(framing->escape != NULL);
    HDLC_ASSERT(framing->transform != NULL);
    HDLC_ASSERT((*framing->frameFlag)() != (*framing->escape)());
    me->framing = framing;
    me->notify = notify;
    me->sink = NULL;
    me->idx = 0U;
    me->chksum = 0U;
    me->state = HDLC_TX_DEAD_;
}

void HdlcEncoder_init(HdlcEncoder *me, HdlcEncoderFrameSink const *sink) {
    HDLC_ASSERT(me != NULL);
    HDLC_ASSERT(me->state == HDLC_TX_DEAD_);
    HDLC_ASSERT(sink != NULL
                && sink->raw != NULL
                && sink->emit.emit != NULL
                && sink->gc.gc != NULL);
    me->sink = sink;
    me->idx = 0U;
    me->chksum = 0U;
    me->state = HDLC_TX_IDLE_;
    HdlcEncoder_notify_(me, HDLC_ENCODER_BUSY);
}

void HdlcEncoder_dispatch(HdlcEncoder *me) {
    HDLC_ASSERT(me != NULL);

    switch (me->state) {
        case HDLC_TX_IDLE_:
            HdlcEncoder_emit_(me, (*me->framing->frameFlag)());
            me->idx = 0U;
            me->state = HDLC_TX_SEQ_;
            break;

        case HDLC_TX_SEQ_: {
            uint8_t const byte = HdlcEncoder_current_(me);
            me->chksum = (uint8_t)(me->chksum + byte);
            if (HdlcEncoder_needsEscape_(me, byte)) {
                HdlcEncoder_emit_(me, (*me->framing->escape)());
                me->state = HDLC_TX_SEQ_ESCAPED_;
            } else {
                HdlcEncoder_emit_(me, byte);
                ++me->idx;
                me->state = HDLC_TX_REC_ID_;
            }
            break;
        }

        case HDLC_TX_SEQ_ESCAPED_:
            HdlcEncoder_emit_(me,
                HdlcEncoder_escape_(me, HdlcEncoder_current_(me)));
            ++me->idx;
            me->state = HDLC_TX_REC_ID_;
            break;

        case HDLC_TX_REC_ID_: {
            uint8_t const byte = HdlcEncoder_current_(me);
            me->chksum = (uint8_t)(me->chksum + byte);
            if (HdlcEncoder_needsEscape_(me, byte)) {
                HdlcEncoder_emit_(me, (*me->framing->escape)());
                me->state = HDLC_TX_REC_ID_ESCAPED_;
            } else {
                HdlcEncoder_emit_(me, byte);
                ++me->idx;
                me->state = HDLC_TX_LENGTH_;
            }
            break;
        }

        case HDLC_TX_REC_ID_ESCAPED_:
            HdlcEncoder_emit_(me, HdlcEncoder_escape_(
                me, HdlcEncoder_current_(me)));
            ++me->idx;
            me->state = HDLC_TX_LENGTH_;
            break;

        case HDLC_TX_LENGTH_: {
            uint8_t const byte = HdlcEncoder_current_(me);
            me->chksum = (uint8_t)(me->chksum + byte);
            if (HdlcEncoder_needsEscape_(me, byte)) {
                HdlcEncoder_emit_(me, (*me->framing->escape)());
                me->state = HDLC_TX_LENGTH_ESCAPED_;
            } else {
                HdlcEncoder_emit_(me, byte);
                ++me->idx;
                me->state = HdlcEncoder_payloadDone_(me)
                            ? HDLC_TX_CHECKSUM_ : HDLC_TX_PAYLOAD_;
            }
            break;
        }

        case HDLC_TX_LENGTH_ESCAPED_:
            HdlcEncoder_emit_(me, HdlcEncoder_escape_(
                me, HdlcEncoder_current_(me)));
            ++me->idx;
            me->state = HdlcEncoder_payloadDone_(me)
                        ? HDLC_TX_CHECKSUM_ : HDLC_TX_PAYLOAD_;
            break;

        case HDLC_TX_PAYLOAD_: {
            uint8_t const byte = HdlcEncoder_current_(me);
            me->chksum = (uint8_t)(me->chksum + byte);
            if (HdlcEncoder_needsEscape_(me, byte)) {
                HdlcEncoder_emit_(me, (*me->framing->escape)());
                me->state = HDLC_TX_PAYLOAD_ESCAPED_;
            } else {
                HdlcEncoder_emit_(me, byte);
                ++me->idx;
                me->state = HdlcEncoder_payloadDone_(me)
                            ? HDLC_TX_CHECKSUM_ : HDLC_TX_PAYLOAD_;
            }
            break;
        }

        case HDLC_TX_PAYLOAD_ESCAPED_:
            HdlcEncoder_emit_(me, HdlcEncoder_escape_(
                me, HdlcEncoder_current_(me)));
            ++me->idx;
            me->state = HdlcEncoder_payloadDone_(me)
                        ? HDLC_TX_CHECKSUM_ : HDLC_TX_PAYLOAD_;
            break;

        case HDLC_TX_CHECKSUM_: {
            me->chksum = (uint8_t)~me->chksum;
            uint8_t const byte = me->chksum;
            if (HdlcEncoder_needsEscape_(me, byte)) {
                HdlcEncoder_emit_(me, (*me->framing->escape)());
                me->state = HDLC_TX_CHECKSUM_ESCAPED_;
            } else {
                HdlcEncoder_emit_(me, byte);
                me->state = HDLC_TX_CLOSE_FLAG_;
            }
            break;
        }

        case HDLC_TX_CHECKSUM_ESCAPED_:
            HdlcEncoder_emit_(me, HdlcEncoder_escape_(
                me, me->chksum));
            me->state = HDLC_TX_CLOSE_FLAG_;
            break;

        case HDLC_TX_CLOSE_FLAG_:
            HdlcEncoder_emit_(me, (*me->framing->frameFlag)());
            me->state = HDLC_TX_INACTIVE_;
            break;

        case HDLC_TX_INACTIVE_: {
            HdlcEncoderFrameSink const *sink = me->sink;
            me->sink = NULL;
            (*sink->gc.gc)(sink->gc.ctx, sink->raw);
            HdlcEncoder_notify_(me, HDLC_ENCODER_READY);
            me->state = HDLC_TX_DEAD_;
            break;
        }

        case HDLC_TX_DEAD_:
            break;

        default:
            HDLC_ASSERT(false);
            break;
    }
}
