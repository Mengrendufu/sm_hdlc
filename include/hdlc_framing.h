//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef HDLC_FRAMING_H_
#define HDLC_FRAMING_H_

#include <stdint.h>

typedef struct {
    uint8_t (*frameFlag)(void);
    uint8_t (*escape)(void);
    uint8_t (*transform)(uint8_t byte);
} HdlcFramingVtbl;

extern HdlcFramingVtbl const HdlcFraming_defaultVtbl;

#endif
