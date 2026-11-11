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

enum {
    APP_FRAME_CAPACITY = 5,
    APP_WIRE_CAPACITY = 2 + 2 * (APP_FRAME_CAPACITY + 1),
};

typedef struct {
    HdlcEncoder encoder;
    HdlcEncoderFrameSink frame;
    uint8_t raw[APP_FRAME_CAPACITY];
    uint8_t wire[APP_WIRE_CAPACITY];
    size_t size;
    unsigned reclaimed;
    bool inputAvailable;
    bool ready;
} App;

static void App_emit_(void *ctx, uint8_t byte) {
    App *app = ctx;
    if (app->size == sizeof(app->wire)) {
        abort();
    }
    app->wire[app->size++] = byte;
}

static void App_gc_(void *ctx, uint8_t const *raw) {
    App *app = ctx;
    if (raw != app->raw || app->encoder.sink != NULL || app->inputAvailable) {
        abort();
    }
    app->inputAvailable = true;
    ++app->reclaimed;
}

static void App_notify_(void *ctx, HdlcEncoderStatus status) {
    App *app = ctx;
    if (status == HDLC_ENCODER_READY && !app->inputAvailable) {
        abort();
    }
    app->ready = status == HDLC_ENCODER_READY;
}

static bool App_encode_(App *app, uint8_t const *expected, size_t size) {
    if (!app->ready || !app->inputAvailable) {
        abort();
    }
    app->size = 0U;
    app->inputAvailable = false;
    HdlcEncoder_init(&app->encoder, &app->frame);
    bool valid = !app->ready && app->size == 0U;
    while (!app->ready) {
        HdlcEncoder_dispatch(&app->encoder);
    }
    valid = valid && app->inputAvailable && app->size == size
            && memcmp(app->wire, expected, size) == 0;
    printf("wire=");
    for (size_t i = 0U; i < app->size; ++i) {
        printf("%s%02X", i == 0U ? "" : " ", (unsigned)app->wire[i]);
    }
    putchar('\n');
    return valid;
}

int main(void) {
    App app = {0};
    app.ready = true;
    app.inputAvailable = true;
    app.frame = (HdlcEncoderFrameSink){
        app.raw, {&App_emit_, &app}, {&App_gc_, &app},
    };
    HdlcEncoder_ctor(&app.encoder, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&App_notify_, &app});

    uint8_t const first[] = {1U, 2U, 2U, 0x7EU, 0x7DU};
    uint8_t const firstWire[] = {
        0x7EU, 1U, 2U, 2U, 0x7DU, 0x5EU, 0x7DU, 0x5DU, 0xFFU, 0x7EU,
    };
    memcpy(app.raw, first, sizeof(first));
    if (!App_encode_(&app, firstWire, sizeof(firstWire))) {
        return EXIT_FAILURE;
    }

    uint8_t const second[] = {5U, 6U, 0U};
    uint8_t const secondWire[] = {0x7EU, 5U, 6U, 0U, 0xF4U, 0x7EU};
    memcpy(app.raw, second, sizeof(second));
    if (!App_encode_(&app, secondWire, sizeof(secondWire))) {
        return EXIT_FAILURE;
    }
    printf("reused=1 gc=%u\n", app.reclaimed);
    return app.reclaimed == 2U ? EXIT_SUCCESS : EXIT_FAILURE;
}
