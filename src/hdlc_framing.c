//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include "hdlc_framing.h"

static uint8_t HdlcFraming_frameFlag_(void) {
    return 0x7EU;
}

static uint8_t HdlcFraming_escape_(void) {
    return 0x7DU;
}

static uint8_t HdlcFraming_transform_(uint8_t byte) {
    return (uint8_t)(byte ^ 0x20U);
}

HdlcFramingVtbl const HdlcFraming_defaultVtbl = {
    &HdlcFraming_frameFlag_,
    &HdlcFraming_escape_,
    &HdlcFraming_transform_,
};
