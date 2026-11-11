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
#include <stdlib.h>
#include <string.h>
#include "hdlc_decoder.h"
#include "hdlc_test_router.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

typedef struct {
    HdlcDecoderFrame frame;
    uint8_t raw[];
} Carrier;

typedef struct {
    HdlcDecoder *decoder;
    Carrier *working;
    HdlcDecoderFrame *received[2];
    size_t capacity;
    unsigned acquired;
    unsigned delivered;
    bool immediate;
    bool valid;
} Owner;

static HdlcDecoderFrame *acquire_(void *ctx) {
    Owner *owner = ctx;
    ++owner->acquired;
    if (owner->working != NULL) {
        abort();
    }
    Carrier *carrier = malloc(sizeof(*carrier) + owner->capacity);
    if (carrier == NULL) {
        abort();
    }
    carrier->frame = (HdlcDecoderFrame){carrier->raw, owner->capacity};
    owner->working = carrier;
    return &carrier->frame;
}

static void emit_(void *ctx, HdlcDecoderFrame *frame) {
    Owner *owner = ctx;
    owner->valid = owner->valid && owner->working != NULL
                   && frame == &owner->working->frame
                   && owner->decoder->frame == NULL
                   && owner->decoder->frameSize == 0U;
    owner->working = NULL;
    if (owner->immediate) {
        free(frame);
    } else {
        if (owner->delivered >= 2U) {
            abort();
        }
        owner->received[owner->delivered] = frame;
    }
    ++owner->delivered;
}

static void feed_(HdlcDecoder *decoder, uint8_t const *wire, size_t size) {
    for (size_t i = 0U; i < size; ++i) {
        HdlcTest_input_(decoder, wire[i]);
    }
}

int main(void) {
    HdlcDecoder decoder;
    Owner owner = {0};
    owner.decoder = &decoder;
    owner.capacity = 3U;
    owner.valid = true;
    HdlcDecoder_ctor(&decoder, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&acquire_, &owner},
                     (HdlcDecoderFrameSink){&emit_, &owner});
    HdlcTest_input_(&decoder, 0x7EU);
    HdlcTest_input_(&decoder, 0x7EU);
    HdlcTest_input_(&decoder, 0x7DU);
    CHECK(owner.acquired == 1U && decoder.frame == &owner.working->frame);
    uint8_t const zero[] = {0x7EU, 1U, 2U, 0U, 0xFCU};
    uint8_t const escaped[] = {
        0x7EU, 0x7DU, 0x5EU, 0x7DU, 0x5DU, 0U, 4U,
    };
    feed_(&decoder, zero, sizeof(zero));
    CHECK(owner.acquired == 1U && owner.delivered == 1U && owner.valid);
    CHECK(owner.received[0]->size == 3U);
    CHECK(memcmp(owner.received[0]->raw, &zero[1], 3U) == 0);
    feed_(&decoder, &escaped[1], sizeof(escaped) - 1U);
    CHECK(owner.acquired == 1U && owner.delivered == 1U);
    feed_(&decoder, escaped, sizeof(escaped));
    CHECK(owner.acquired == 2U && owner.delivered == 2U && owner.valid);
    CHECK(owner.received[0] != owner.received[1]);
    CHECK(memcmp(owner.received[0]->raw, &zero[1], 3U) == 0);
    CHECK(owner.received[1]->raw[0] == 0x7EU);
    CHECK(owner.received[1]->raw[1] == 0x7DU);
    free(owner.received[0]);
    free(owner.received[1]);
    owner.immediate = true;
    uint8_t const escapedChecksum[] = {
        0x7EU, 1U, 0x80U, 0U, 0x7DU, 0x5EU,
    };
    feed_(&decoder, escapedChecksum, sizeof(escapedChecksum));
    CHECK(owner.working == NULL && owner.valid);

    for (unsigned length = 0U; length <= UINT8_MAX; ++length) {
        owner.capacity = 3U + length;
        uint8_t header[] = {0x7EU, 1U, 2U, (uint8_t)length};
        uint8_t sum = (uint8_t)(3U + length);
        HdlcDecoderEvt event = {HDLC_DECODER_FRAME_FLAG_SIG, 0U};
        HdlcDecoder_dispatch(&decoder, &event);
        for (size_t i = 1U; i < sizeof(header); ++i) {
            event = (HdlcDecoderEvt){HDLC_DECODER_BYTE_SIG, header[i]};
            HdlcDecoder_dispatch(&decoder, &event);
        }
        for (unsigned i = 0U; i < length; ++i) {
            event = (HdlcDecoderEvt){HDLC_DECODER_BYTE_SIG, (uint8_t)i};
            sum = (uint8_t)(sum + i);
            HdlcDecoder_dispatch(&decoder, &event);
        }
        CHECK(decoder.frameSize == owner.capacity);
        CHECK(decoder.frame != NULL);
        event = (HdlcDecoderEvt){HDLC_DECODER_BYTE_SIG, (uint8_t)~sum};
        HdlcDecoder_dispatch(&decoder, &event);
        CHECK(decoder.frame == NULL && owner.working == NULL);
        CHECK(owner.valid);
    }

    owner.capacity = 3U;
    uint8_t const oversized[] = {0x7EU, 1U, 2U, 1U, 3U, 0xF8U};
    unsigned acquired = owner.acquired;
    unsigned delivered = owner.delivered;
    feed_(&decoder, oversized, sizeof(oversized));
    CHECK(owner.acquired == acquired + 1U && owner.delivered == delivered);
    CHECK(decoder.frame == &owner.working->frame);
    CHECK(decoder.frameSize == 0U && decoder.frame->size == 3U);
    feed_(&decoder, &zero[1], sizeof(zero) - 1U);
    CHECK(owner.delivered == delivered);
    feed_(&decoder, zero, sizeof(zero));
    CHECK(owner.acquired == acquired + 1U);
    CHECK(owner.delivered == delivered + 1U && owner.valid);

    uint8_t const oversizedEscaped[] = {
        0x7EU, 1U, 2U, 0x7DU, 0x5DU,
    };
    acquired = owner.acquired;
    delivered = owner.delivered;
    feed_(&decoder, oversizedEscaped, sizeof(oversizedEscaped));
    CHECK(owner.acquired == acquired + 1U);
    CHECK(owner.delivered == delivered && decoder.frameSize == 0U);
    feed_(&decoder, zero, sizeof(zero));
    CHECK(owner.acquired == acquired + 1U);
    CHECK(owner.delivered == delivered + 1U && owner.valid);

    uint8_t const bad[] = {0x7EU, 1U, 2U, 0U, 0U};
    acquired = owner.acquired;
    delivered = owner.delivered;
    feed_(&decoder, bad, sizeof(bad));
    CHECK(owner.working != NULL && owner.delivered == delivered);
    feed_(&decoder, &zero[1], sizeof(zero) - 1U);
    CHECK(owner.acquired == acquired + 1U);
    CHECK(owner.delivered == delivered);
    feed_(&decoder, zero, sizeof(zero));
    CHECK(owner.acquired == acquired + 1U);
    CHECK(owner.delivered == delivered + 1U);

    feed_(&decoder, zero, 3U);
    Carrier *working = owner.working;
    uint8_t const broken[] = {0x7DU, 0x7DU};
    feed_(&decoder, broken, sizeof(broken));
    CHECK(owner.working == working && decoder.frameSize == 0U);
    feed_(&decoder, zero, sizeof(zero));
    CHECK(owner.working == NULL && owner.valid);

    owner.capacity = 5U;
    uint8_t const payload[] = {
        0x7EU, 1U, 2U, 2U, 0x7DU, 0x5EU, 0x7DU, 0x5DU, 0xFFU,
    };
    feed_(&decoder, payload, sizeof(payload) - 1U);
    CHECK(decoder.frameSize == 5U && decoder.frame->size == 5U);
    CHECK(decoder.frame->raw[3] == 0x7EU);
    CHECK(decoder.frame->raw[4] == 0x7DU);
    feed_(&decoder, &payload[sizeof(payload) - 1U], 1U);
    CHECK(owner.working == NULL && owner.valid);
    owner.capacity = 3U;

    feed_(&decoder, zero, 2U);
    working = owner.working;
    HdlcDecoder_ctor(&decoder, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&acquire_, &owner},
                     (HdlcDecoderFrameSink){&emit_, &owner});
    CHECK(decoder.frame == NULL && decoder.frameSize == 0U);
    CHECK(owner.working == working);
    free(working);
    owner.working = NULL;
    acquired = owner.acquired;
    delivered = owner.delivered;
    feed_(&decoder, &zero[1], sizeof(zero) - 1U);
    CHECK(owner.acquired == acquired && decoder.frame == NULL);
    CHECK(owner.delivered == delivered);
    HdlcTest_input_(&decoder, 0x7EU);
    CHECK(owner.acquired == acquired + 1U && decoder.frame != NULL);
    working = owner.working;
    HdlcTest_input_(&decoder, 0x7EU);
    CHECK(owner.acquired == acquired + 1U && owner.working == working);
    CHECK(decoder.frameSize == 0U && decoder.checksum == 0U);
    feed_(&decoder, &zero[1], sizeof(zero) - 1U);
    CHECK(owner.delivered == delivered + 1U && owner.valid);
    feed_(&decoder, zero, sizeof(zero));
    CHECK(owner.acquired == acquired + 2U);
    CHECK(owner.delivered == delivered + 2U && owner.valid);
    return 0;
}
