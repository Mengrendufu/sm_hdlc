//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hdlc_decoder.h"

enum {
    APP_PAYLOAD_CAPACITY = 16,
    APP_FRAME_CAPACITY = 3 + APP_PAYLOAD_CAPACITY,
    APP_RESULT_CAPACITY = 2,
};

typedef struct {
    uint8_t raw[APP_FRAME_CAPACITY];
    size_t size;
} AppResult;

typedef struct {
    HdlcDecoder decoder;
    HdlcDecoderFrame carrier;
    uint8_t working[APP_FRAME_CAPACITY];
    AppResult results[APP_RESULT_CAPACITY];
    size_t count;
} App;

static HdlcDecoderFrame *App_acquire_(void *ctx) {
    App *app = ctx;
    return &app->carrier;
}

static void App_receive_(void *ctx, HdlcDecoderFrame *frame) {
    App *app = ctx;
    size_t const size = 3U + frame->raw[2];
    if (app->count == APP_RESULT_CAPACITY || size > APP_FRAME_CAPACITY) {
        abort();
    }
    AppResult *result = &app->results[app->count];
    memcpy(result->raw, frame->raw, size);
    result->size = size;
    ++app->count;
}

static void App_input_(App *app, uint8_t const *bytes, size_t size) {
    HdlcFramingVtbl const *framing = &HdlcFraming_defaultVtbl;
    for (size_t i = 0U; i < size; ++i) {
        uint8_t const byte = bytes[i];
        HdlcDecoderEvt const event = {
            .sig = byte == (*framing->frameFlag)()
                   ? HDLC_DECODER_FRAME_FLAG_SIG
                   : (byte == (*framing->escape)()
                      ? HDLC_DECODER_ESCAPE_SIG : HDLC_DECODER_BYTE_SIG),
            .byte = byte,
        };
        HdlcDecoder_dispatch(&app->decoder, &event);
    }
}

static void App_process_(AppResult const *result) {
    printf("seq=%u recID=%u payload=", (unsigned)result->raw[0],
           (unsigned)result->raw[1]);
    for (size_t i = 3U; i < result->size; ++i) {
        printf("%s%02X", i == 3U ? "" : " ", (unsigned)result->raw[i]);
    }
    putchar('\n');
}

int main(void) {
    App app = {0};
    app.carrier = (HdlcDecoderFrame){app.working, sizeof(app.working)};
    HdlcDecoder_ctor(&app.decoder, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&App_acquire_, &app},
                     (HdlcDecoderFrameSink){&App_receive_, &app});

    uint8_t const packetA[] = {0x7EU, 1U, 2U, 2U, 0x7DU};
    uint8_t const packetB[] = {
        0x5EU, 0x7DU, 0x5DU, 0xFFU, 0x7EU,
        3U, 4U, 0U, 0U, 0x7EU,
        5U, 6U, 0U, 0xF4U, 0x7EU,
    };
    App_input_(&app, packetA, sizeof(packetA));
    if (app.count != 0U) {
        return EXIT_FAILURE;
    }
    App_input_(&app, packetB, sizeof(packetB));

    uint8_t const first[] = {1U, 2U, 2U, 0x7EU, 0x7DU};
    uint8_t const second[] = {5U, 6U, 0U};
    if (app.count != 2U
        || app.results[0].size != sizeof(first)
        || memcmp(app.results[0].raw, first, sizeof(first)) != 0
        || app.results[1].size != sizeof(second)
        || memcmp(app.results[1].raw, second, sizeof(second)) != 0)
    {
        return EXIT_FAILURE;
    }
    for (size_t i = 0U; i < app.count; ++i) {
        App_process_(&app.results[i]);
    }
    return EXIT_SUCCESS;
}
