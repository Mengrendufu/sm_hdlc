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

typedef struct {
    uint8_t wire[14];
    size_t size;
} AppOutput;

typedef struct {
    HdlcEncoder encoder;
    AppOutput output;
    unsigned reclaimed;
    bool ready;
} App;

typedef struct {
    HdlcEncoderFrameSink sink;
    App *owner;
    uint8_t raw[];
} AppFrame;

static void AppOutput_emit_(void *ctx, uint8_t byte) {
    AppOutput *output = ctx;
    if (output->size == sizeof(output->wire)) {
        abort();
    }
    output->wire[output->size++] = byte;
}

static void AppFrame_gc_(void *ctx, uint8_t const *raw) {
    AppFrame *frame = ctx;
    App *app = frame->owner;
    if (raw != frame->raw || app->encoder.sink != NULL) {
        abort();
    }
    ++app->reclaimed;
    free(frame);
}

static void App_notify_(void *ctx, HdlcEncoderStatus status) {
    App *app = ctx;
    if (status == HDLC_ENCODER_READY && app->reclaimed != 1U) {
        abort();
    }
    app->ready = status == HDLC_ENCODER_READY;
}

int main(void) {
    App app = {0};
    HdlcEncoder_ctor(&app.encoder, &HdlcFraming_defaultVtbl,
                     (HdlcEncoderNotifySink){&App_notify_, &app});
    uint8_t const raw[] = {1U, 2U, 2U, 0x7EU, 0x7DU};
    uint8_t const expected[] = {
        0x7EU, 1U, 2U, 2U, 0x7DU, 0x5EU, 0x7DU, 0x5DU, 0xFFU, 0x7EU,
    };
    AppFrame *frame = malloc(sizeof(*frame) + sizeof(raw));
    if (frame == NULL) {
        return EXIT_FAILURE;
    }
    frame->owner = &app;
    memcpy(frame->raw, raw, sizeof(raw));
    frame->sink = (HdlcEncoderFrameSink){
        frame->raw, {&AppOutput_emit_, &app.output}, {&AppFrame_gc_, frame},
    };
    HdlcEncoder_init(&app.encoder, &frame->sink);
    frame = NULL;
    bool valid = !app.ready && app.output.size == 0U;
    for (size_t i = 0U; i < sizeof(expected); ++i) {
        HdlcEncoder_dispatch(&app.encoder);
        valid = valid && app.output.size == i + 1U
                && !app.ready && app.reclaimed == 0U;
    }
    HdlcEncoder_dispatch(&app.encoder);
    valid = valid && app.ready && app.reclaimed == 1U
            && app.encoder.sink == NULL && app.output.size == sizeof(expected)
            && memcmp(app.output.wire, expected, sizeof(expected)) == 0;
    HdlcEncoder_dispatch(&app.encoder);
    valid = valid && app.reclaimed == 1U
            && app.output.size == sizeof(expected);

    printf("wire=");
    for (size_t i = 0U; i < app.output.size; ++i) {
        printf("%s%02X", i == 0U ? "" : " ", (unsigned)app.output.wire[i]);
    }
    printf("\nreclaimed=%u ready=%u\n", app.reclaimed, (unsigned)app.ready);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
