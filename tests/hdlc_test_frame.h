//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_TEST_FRAME_H_
#define HDLC_TEST_FRAME_H_

#include <stddef.h>
#include <stdint.h>

static size_t HdlcTest_buildFrame_(uint8_t *frame,
                                   uint8_t seq,
                                   uint8_t recId,
                                   uint8_t const *payload,
                                   size_t payloadSize)
{
    frame[0] = seq;
    frame[1] = recId;
    frame[2] = (uint8_t)payloadSize;
    uint8_t checksum = (uint8_t)(seq + recId + payloadSize);
    for (size_t i = 0U; i < payloadSize; ++i) {
        frame[3U + i] = payload[i];
        checksum = (uint8_t)(checksum + payload[i]);
    }
    frame[3U + payloadSize] = (uint8_t)(0xFFU - checksum);
    return 4U + payloadSize;
}

#endif
