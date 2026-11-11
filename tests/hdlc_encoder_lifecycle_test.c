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
#include "hdlc_encoder.h"
#include "hdlc_test_frame.h"

#define CHECK(expr_) do { \
    if (!(expr_)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr_); \
        return 1; \
    } \
} while (0)

typedef struct {
    HdlcEncoder *encoder;
    uint8_t wire[520];
    size_t size;
    unsigned gc;
    unsigned busy;
    unsigned ready;
    bool valid;
    bool release;
} Owner;

typedef struct {
    HdlcEncoderFrameSink sink;
    Owner *owner;
    uint8_t raw[];
} Carrier;

static void emit_(void *ctx, uint8_t byte) {
    Owner *owner = ctx;
    if (owner->size >= sizeof(owner->wire)) {
        abort();
    }
    owner->wire[owner->size++] = byte;
}

static void gc_(void *ctx, uint8_t const *raw) {
    Owner *owner = ctx;
    owner->valid = owner->valid && raw != NULL
                   && owner->encoder->sink == NULL;
    ++owner->gc;
    if (owner->release) {
        free((void *)raw);
    }
}

static void notify_(void *ctx, HdlcEncoderStatus status) {
    Owner *owner = ctx;
    if (status == HDLC_ENCODER_BUSY) {
        ++owner->busy;
        owner->valid = owner->valid
                       && owner->encoder->sink != NULL;
    } else {
        ++owner->ready;
        owner->valid = owner->valid
                       && owner->encoder->sink == NULL
                       && owner->gc == owner->ready;
    }
}

static void reclaimCarrier_(void *ctx, uint8_t const *raw) {
    Carrier *carrier = ctx;
    Owner *owner = carrier->owner;
    owner->valid = owner->valid && raw == carrier->raw
                   && owner->encoder->sink == NULL;
    ++owner->gc;
    free(carrier);
}

int main(void) {
    HdlcEncoder encoder;
    Owner owner = {0};
    owner.encoder = &encoder;
    owner.valid = true;
    HdlcEncoder_ctor(&encoder, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&notify_, &owner});
    CHECK(encoder.sink == NULL);
    HdlcEncoder_dispatch(&encoder);
    CHECK(owner.size == 0U && owner.gc == 0U);

    uint8_t const raw[] = {1U, 2U, 2U, 0x7EU, 0x7DU};
    uint8_t const expected[] = {
        0x7EU, 1U, 2U, 2U, 0x7DU, 0x5EU, 0x7DU, 0x5DU,
        0xFFU, 0x7EU,
    };
    HdlcEncoderFrameSink frame = {raw, {&emit_, &owner}, {&gc_, &owner}};
    HdlcEncoder_init(&encoder, &frame);
    CHECK(owner.busy == 1U && owner.ready == 0U);
    CHECK(owner.size == 0U && encoder.sink == &frame);
    CHECK(encoder.idx == 0U && encoder.chksum == 0U);
    for (size_t i = 0U; i < sizeof(expected); ++i) {
        HdlcEncoder_dispatch(&encoder);
        CHECK(owner.size == i + 1U);
        CHECK(owner.wire[i] == expected[i]);
        CHECK(owner.gc == 0U && owner.ready == 0U);
    }
    CHECK(encoder.sink == &frame);
    HdlcEncoder_dispatch(&encoder);
    CHECK(owner.gc == 1U && owner.ready == 1U && owner.valid);
    CHECK(owner.size == sizeof(expected) && encoder.sink == NULL);
    for (unsigned i = 0U; i < 10U; ++i) {
        HdlcEncoder_dispatch(&encoder);
    }
    CHECK(owner.gc == 1U && owner.ready == 1U);

    uint8_t *heap = malloc(3U);
    CHECK(heap != NULL);
    heap[0] = 0U;
    heap[1] = 0U;
    heap[2] = 0U;
    owner.release = true;
    owner.size = 0U;
    HdlcEncoderFrameSink allocated = {
        heap, {&emit_, &owner}, {&gc_, &owner},
    };
    HdlcEncoder_init(&encoder, &allocated);
    while (owner.ready != owner.busy) {
        HdlcEncoder_dispatch(&encoder);
    }
    CHECK(owner.size == 6U && owner.valid);
    CHECK(owner.ready == owner.busy && owner.ready == owner.gc);
    CHECK(encoder.sink == NULL);

    for (unsigned size = 0U; size <= UINT8_MAX; ++size) {
        Carrier *carrier = malloc(sizeof(*carrier) + 3U + size);
        CHECK(carrier != NULL);
        carrier->owner = &owner;
        carrier->raw[0] = 0x7DU;
        carrier->raw[1] = 0x7EU;
        carrier->raw[2] = (uint8_t)size;
        for (size_t i = 0U; i < size; ++i) {
            carrier->raw[3U + i] = (uint8_t)(size + i);
        }
        carrier->sink = (HdlcEncoderFrameSink){
            carrier->raw, {&emit_, &owner}, {&reclaimCarrier_, carrier},
        };
        uint8_t body[4U + UINT8_MAX];
        size_t count = HdlcTest_buildFrame_(body, 0x7DU, 0x7EU,
                                           &carrier->raw[3], size);
        uint8_t wire[520];
        size_t n = 0U;
        wire[n++] = 0x7EU;
        for (size_t i = 0U; i < count; ++i) {
            uint8_t byte = body[i];
            if (byte == 0x7DU || byte == 0x7EU) {
                wire[n++] = 0x7DU;
                byte ^= 0x20U;
            }
            wire[n++] = byte;
        }
        wire[n++] = 0x7EU;
        owner.size = 0U;
        HdlcEncoder_init(&encoder, &carrier->sink);
        while (owner.ready != owner.busy) {
            HdlcEncoder_dispatch(&encoder);
        }
        CHECK(owner.size == n && memcmp(owner.wire, wire, n) == 0);
        CHECK(encoder.idx == 3U + size && encoder.sink == NULL);
        CHECK(owner.ready == owner.gc && owner.busy == owner.gc);
        CHECK(owner.valid);
        HdlcEncoder_dispatch(&encoder);
        CHECK(owner.size == n && owner.ready == owner.gc);
    }
    return 0;
}
