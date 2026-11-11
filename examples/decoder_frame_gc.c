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

enum {
    APP_FRAME_CAPACITY = 3 + 16,
    APP_RESULT_CAPACITY = 2,
};

typedef struct AppFrame AppFrame;
struct AppFrame {
    HdlcDecoderFrame frame;
    void (*gc)(AppFrame *frame);
    unsigned *reclaimed;
    uint8_t raw[];
};

typedef struct {
    HdlcDecoder decoder;
    AppFrame *working;
    AppFrame *results[APP_RESULT_CAPACITY];
    size_t count;
    unsigned allocated;
    unsigned reclaimed;
} App;

static void AppFrame_gc_(AppFrame *frame) {
    ++*frame->reclaimed;
    free(frame);
}

static HdlcDecoderFrame *App_acquire_(void *ctx) {
    App *app = ctx;
    if (app->working != NULL) {
        abort();
    }
    AppFrame *frame = malloc(sizeof(*frame) + APP_FRAME_CAPACITY);
    if (frame == NULL) {
        abort();
    }
    frame->frame = (HdlcDecoderFrame){frame->raw, APP_FRAME_CAPACITY};
    frame->gc = &AppFrame_gc_;
    frame->reclaimed = &app->reclaimed;
    app->working = frame;
    ++app->allocated;
    return &frame->frame;
}

static void App_receive_(void *ctx, HdlcDecoderFrame *frame) {
    App *app = ctx;
    if (app->working == NULL || frame != &app->working->frame
        || app->count == APP_RESULT_CAPACITY)
    {
        abort();
    }
    app->results[app->count++] = app->working;
    app->working = NULL;
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

static void AppFrame_process_(AppFrame const *frame) {
    uint8_t const *raw = frame->frame.raw;
    printf("seq=%u recID=%u payload=", (unsigned)raw[0], (unsigned)raw[1]);
    for (size_t i = 3U; i < 3U + raw[2]; ++i) {
        printf("%s%02X", i == 3U ? "" : " ", (unsigned)raw[i]);
    }
    putchar('\n');
}

static void App_stop_(App *app) {
    HdlcDecoder_ctor(&app->decoder, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&App_acquire_, app},
                     (HdlcDecoderFrameSink){&App_receive_, app});
    AppFrame *frame = app->working;
    app->working = NULL;
    if (frame != NULL) {
        (*frame->gc)(frame);
    }
}

int main(void) {
    App app = {0};
    HdlcDecoder_ctor(&app.decoder, &HdlcFraming_defaultVtbl,
                     (HdlcDecoderAcquireSink){&App_acquire_, &app},
                     (HdlcDecoderFrameSink){&App_receive_, &app});
    uint8_t const wire[] = {
        0x7EU, 1U, 2U, 2U, 0x7DU, 0x5EU, 0x7DU, 0x5DU, 0xFFU, 0x7EU,
        3U, 4U, 0U, 0U, 0x7EU,
        5U, 6U, 0U, 0xF4U, 0x7EU,
        7U, 8U,
    };
    App_input_(&app, wire, sizeof(wire));

    uint8_t const first[] = {1U, 2U, 2U, 0x7EU, 0x7DU};
    uint8_t const second[] = {5U, 6U, 0U};
    bool valid = app.count == 2U && app.allocated == 3U
                 && app.reclaimed == 0U && app.working != NULL;
    if (valid) {
        valid = 3U + app.results[0]->frame.raw[2] == sizeof(first)
                && memcmp(app.results[0]->frame.raw, first, sizeof(first)) == 0
                && 3U + app.results[1]->frame.raw[2] == sizeof(second)
                && memcmp(app.results[1]->frame.raw, second, sizeof(second)) == 0;
    }
    for (size_t i = 0U; i < app.count; ++i) {
        AppFrame *frame = app.results[i];
        app.results[i] = NULL;
        AppFrame_process_(frame);
        (*frame->gc)(frame);
    }
    app.count = 0U;
    App_stop_(&app);
    printf("allocated=%u reclaimed=%u\n", app.allocated, app.reclaimed);
    return valid && app.reclaimed == app.allocated
           ? EXIT_SUCCESS : EXIT_FAILURE;
}
