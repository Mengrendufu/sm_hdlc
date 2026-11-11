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
#include "hdlc_decoder.h"

enum HdlcDecoderState_ {
    HDLC_RX_IDLE_,
    HDLC_RX_WAIT_SEQ_,
    HDLC_RX_WAIT_SEQ_ESCAPED_,
    HDLC_RX_WAIT_REC_ID_,
    HDLC_RX_WAIT_REC_ID_ESCAPED_,
    HDLC_RX_WAIT_LENGTH_,
    HDLC_RX_WAIT_LENGTH_ESCAPED_,
    HDLC_RX_WAIT_PAYLOAD_,
    HDLC_RX_WAIT_PAYLOAD_ESCAPED_,
    HDLC_RX_WAIT_CHECKSUM_,
    HDLC_RX_WAIT_CHECKSUM_ESCAPED_,
};

static void HdlcDecoder_clearFrame_(HdlcDecoder *me) {
    me->frameSize = 0U;
    me->payloadRemaining = 0U;
    me->checksum = 0U;
}

static void HdlcDecoder_append_(HdlcDecoder *me, uint8_t byte) {
    HDLC_ASSERT(me->frameSize < me->frame->size);
    me->frame->raw[me->frameSize++] = byte;
    me->checksum = (uint8_t)(me->checksum + byte);
}

static void HdlcDecoder_enterActive_(HdlcDecoder *me) {
    if (me->frame == NULL) {
        me->frame = (*me->acquire.acquire)(me->acquire.ctx);
        HDLC_ASSERT(me->frame != NULL
                    && me->frame->raw != NULL
                    && me->frame->size >= 3U);
    }
    HdlcDecoder_clearFrame_(me);
}

void HdlcDecoder_ctor(HdlcDecoder *me,
                      HdlcFramingVtbl const *framing,
                      HdlcDecoderAcquireSink acquire,
                      HdlcDecoderFrameSink sink)
{
    HDLC_ASSERT(me != NULL);
    HDLC_ASSERT(acquire.acquire != NULL);
    HDLC_ASSERT(sink.emit != NULL);
    HDLC_ASSERT(framing != NULL);
    HDLC_ASSERT(framing->frameFlag != NULL);
    HDLC_ASSERT(framing->escape != NULL);
    HDLC_ASSERT(framing->transform != NULL);
    HDLC_ASSERT((*framing->frameFlag)() != (*framing->escape)());
    me->framing = framing;
    me->acquire = acquire;
    me->sink = sink;
    me->frame = NULL;
    HdlcDecoder_clearFrame_(me);
    me->state = HDLC_RX_IDLE_;
}

void HdlcDecoder_dispatch(HdlcDecoder *me, HdlcDecoderEvt const *e) {
    HDLC_ASSERT(me != NULL);
    HDLC_ASSERT(e != NULL);
    HDLC_ASSERT(e->sig == HDLC_DECODER_FRAME_FLAG_SIG
                || e->sig == HDLC_DECODER_ESCAPE_SIG
                || e->sig == HDLC_DECODER_BYTE_SIG);

    switch (me->state) {
        case HDLC_RX_IDLE_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            }
            break;

        case HDLC_RX_WAIT_SEQ_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                me->state = HDLC_RX_WAIT_SEQ_ESCAPED_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, e->byte);
                me->state = HDLC_RX_WAIT_REC_ID_;
            }
            break;

        case HDLC_RX_WAIT_SEQ_ESCAPED_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                HdlcDecoder_clearFrame_(me);
                me->state = HDLC_RX_IDLE_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, (*me->framing->transform)(e->byte));
                me->state = HDLC_RX_WAIT_REC_ID_;
            }
            break;

        case HDLC_RX_WAIT_REC_ID_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                me->state = HDLC_RX_WAIT_REC_ID_ESCAPED_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, e->byte);
                me->state = HDLC_RX_WAIT_LENGTH_;
            }
            break;

        case HDLC_RX_WAIT_REC_ID_ESCAPED_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                HdlcDecoder_clearFrame_(me);
                me->state = HDLC_RX_IDLE_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, (*me->framing->transform)(e->byte));
                me->state = HDLC_RX_WAIT_LENGTH_;
            }
            break;

        case HDLC_RX_WAIT_LENGTH_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                me->state = HDLC_RX_WAIT_LENGTH_ESCAPED_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, e->byte);
                me->payloadRemaining = e->byte;
                if (3U + me->payloadRemaining > me->frame->size) {
                    HdlcDecoder_clearFrame_(me);
                    me->state = HDLC_RX_IDLE_;
                } else if (me->payloadRemaining == 0U) {
                    me->state = HDLC_RX_WAIT_CHECKSUM_;
                } else {
                    me->state = HDLC_RX_WAIT_PAYLOAD_;
                }
            }
            break;

        case HDLC_RX_WAIT_LENGTH_ESCAPED_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                HdlcDecoder_clearFrame_(me);
                me->state = HDLC_RX_IDLE_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                uint8_t const length = (*me->framing->transform)(e->byte);
                HdlcDecoder_append_(me, length);
                me->payloadRemaining = length;
                if (3U + me->payloadRemaining > me->frame->size) {
                    HdlcDecoder_clearFrame_(me);
                    me->state = HDLC_RX_IDLE_;
                } else if (me->payloadRemaining == 0U) {
                    me->state = HDLC_RX_WAIT_CHECKSUM_;
                } else {
                    me->state = HDLC_RX_WAIT_PAYLOAD_;
                }
            }
            break;

        case HDLC_RX_WAIT_PAYLOAD_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                me->state = HDLC_RX_WAIT_PAYLOAD_ESCAPED_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, e->byte);
                --me->payloadRemaining;
                if (me->payloadRemaining == 0U) {
                    me->state = HDLC_RX_WAIT_CHECKSUM_;
                }
            }
            break;

        case HDLC_RX_WAIT_PAYLOAD_ESCAPED_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                HdlcDecoder_clearFrame_(me);
                me->state = HDLC_RX_IDLE_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                HdlcDecoder_append_(me, (*me->framing->transform)(e->byte));
                --me->payloadRemaining;
                if (me->payloadRemaining == 0U) {
                    me->state = HDLC_RX_WAIT_CHECKSUM_;
                } else {
                    me->state = HDLC_RX_WAIT_PAYLOAD_;
                }
            }
            break;

        case HDLC_RX_WAIT_CHECKSUM_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                me->state = HDLC_RX_WAIT_CHECKSUM_ESCAPED_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                if ((uint8_t)(me->checksum + e->byte) == 0xFFU) {
                    HdlcDecoderFrame *frame = me->frame;
                    me->frame = NULL;
                    HdlcDecoder_clearFrame_(me);
                    me->state = HDLC_RX_IDLE_;
                    (*me->sink.emit)(me->sink.ctx, frame);
                } else {
                    HdlcDecoder_clearFrame_(me);
                    me->state = HDLC_RX_IDLE_;
                }
            }
            break;

        case HDLC_RX_WAIT_CHECKSUM_ESCAPED_:
            if (e->sig == HDLC_DECODER_FRAME_FLAG_SIG) {
                HdlcDecoder_enterActive_(me);
                me->state = HDLC_RX_WAIT_SEQ_;
            } else if (e->sig == HDLC_DECODER_ESCAPE_SIG) {
                HdlcDecoder_clearFrame_(me);
                me->state = HDLC_RX_IDLE_;
            } else if (e->sig == HDLC_DECODER_BYTE_SIG) {
                uint8_t const checksum = (*me->framing->transform)(e->byte);
                if ((uint8_t)(me->checksum + checksum) == 0xFFU) {
                    HdlcDecoderFrame *frame = me->frame;
                    me->frame = NULL;
                    HdlcDecoder_clearFrame_(me);
                    me->state = HDLC_RX_IDLE_;
                    (*me->sink.emit)(me->sink.ctx, frame);
                } else {
                    HdlcDecoder_clearFrame_(me);
                    me->state = HDLC_RX_IDLE_;
                }
            }
            break;

        default:
            HDLC_ASSERT(false);
            break;
    }

}
