# sm_hdlc

## Scope and build

Standalone C99 encoder and decoder for the existing sm_tracer wire protocol.
No heap, OS, sm_hsm, SST, transport, queue, or UI dependency.

- `sm_hdlc_decoder`: `include/hdlc_decoder.h`, `src/hdlc_decoder.c`.
- `sm_hdlc_encoder`: `include/hdlc_encoder.h`, `src/hdlc_encoder.c`.
- `sm_hdlc_framing`: `include/hdlc_framing.h`, `src/hdlc_framing.c`;
  public common dependency of both encoder and decoder targets. Neither
  depends on the other's header or implementation.
- Consumers can use either CMake target independently via `add_subdirectory`.
- When included as a subdirectory, this module does not register its tests.

Verification:

```sh
cmake -S . -B /tmp/sm-hdlc-build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build /tmp/sm-hdlc-build-debug
ctest --test-dir /tmp/sm-hdlc-build-debug --output-on-failure
```

The standalone build includes `hdlc_decoder_fixed_buffer` from
`examples/decoder_fixed_buffer.c`; it is also registered with CTest when testing
is enabled. Run `/tmp/sm-hdlc-build-debug/hdlc_decoder_fixed_buffer` to see decoded
results. The example owns one fixed working buffer and copies valid frames into
two owner-managed result slots before processing them after input dispatch.
It demonstrates a frame split across packets at an escape prefix, multiple
frames in one packet, invalid-checksum rejection, and a zero-length payload.
Result slots are a bounded, single-threaded demonstration, not a concurrent
queue; exhausting them is a violated example input contract and aborts.
Library-only `add_subdirectory` consumers do not build examples.

`examples/decoder_frame_gc.c` builds as `hdlc_decoder_frame_gc` and is registered
with CTest. Run `/tmp/sm-hdlc-build-debug/hdlc_decoder_frame_gc`. Its application
carrier wraps the decoder descriptor, a GC function pointer, a borrowed
reclamation-counter pointer, and inline raw storage in one heap allocation.
Successful delivery transfers the carrier into two bounded result slots without
copying. Deferred processing explicitly calls the carrier GC. Bad checksum
reuses the working allocation. With input quiesced, App_stop_ reconstructs the
decoder to detach a partial frame, then explicitly reclaims the owner's tracked
working carrier. The App and its counter outlive all carriers; the decoder
itself still has no GC interface. This is a single-threaded deferred-consumer
example, not a thread-safe queue.

`examples/encoder_fixed_buffer.c` builds as `hdlc_encoder_fixed_buffer`.
It owns one fixed raw buffer, one descriptor, and a separate wire buffer.
GC marks the input reusable without freeing it; READY controls the owner-side
dispatch loop. The same buffers are reused for an escaped payload and then a
zero-payload frame, after the previous output has been processed.
`examples/encoder_frame_gc.c` builds as `hdlc_encoder_frame_gc`. The owner
allocates a descriptor and inline raw storage together, submits it with init,
and no longer accesses that carrier. Its GC checks encoder detachment and
frees the whole allocation; output storage and the notify context remain alive.
The example verifies one wire byte per encoding dispatch, no GC/READY at the
closing flag, and GC followed by READY on the extra completion dispatch.
Both executables are registered with CTest. Run them from the selected build
directory. They use synchronous memory sinks, not asynchronous transport;
physical transmission completion and output-buffer reuse remain owner duties.

## Protocol and contracts

- Default wire format: `0x7E Seq RecID Length Payload Checksum 0x7E`.
- Default framing escapes `0x7D` and `0x7E` between the flags as
  `0x7D (byte ^ 0x20)`. Length is one byte, with 0..255 payload bytes.
- The unescaped sum from Seq through Checksum is `0xFF` modulo 256.
- Decoder callbacks expose `Seq RecID Length Payload`, not flags/checksum.
- Construct each instance before use. Instance fields are internal state;
  callers must not modify them or reconstruct an instance during an operation.
- Decoder construction is immediately ready for dispatch; no separate init.
  Its only public operations are ctor and dispatch. Initialization is expanded
  in ctor: detach any carrier, clear parsing progress, and wait for a new flag.
  To reinitialize, quiesce dispatch and call ctor with the required configuration;
  the owner remains responsible for the detached carrier's memory.
- Decoder construction takes two interface values: `HdlcDecoderAcquireSink`
  contains acquire/ctx, and `HdlcDecoderFrameSink` contains emit/ctx. Both values
  are copied into the instance, so their temporary wrapper objects need not
  survive construction. Both callbacks are required; contexts may be null if
  their callbacks permit and otherwise must outlive their use. Acquire returns
  an owner-provided `HdlcDecoderFrame` containing writable raw and size. Size is
  the writable capacity starting at raw, fixed before acquisition, not the
  allocation's metadata size or the decoded frame's valid length.
- A successful acquire must provide non-null raw and size >= 3. Descriptor and
  storage remain valid, with raw/size unchanged, until delivery or reconstruction.
  The decoder has no internal frame array, allocator, or GC interface.
- Active entry conditionally acquires a carrier when frame is null, asserts
  non-null frame/raw and size >= 3 only for a newly acquired carrier, then clears
  parsing progress. Every flag
  enters or re-enters active and selects waitSeq; repeated flags reuse the
  existing carrier without another acquire. Even a trailing flag may acquire
  the next working carrier before any Seq arrives.
  Invalid checksum, interrupted frames, and illegal escapes clear progress but
  retain the working carrier for reuse. Acquire is called again only when no
  carrier is mounted. The owner must track supplied storage, including a carrier
  detached by reconstruction, and manage its reclamation without a callback.
- On valid checksum, the decoder detaches its carrier, clears progress, enters
  idle, then hands that descriptor to the frame sink. It does not touch the
  descriptor/storage afterward. The sink accepts the handoff unconditionally
  and may retain, recycle, or free the carrier, including inside the callback.
  Further frames can use different carriers while previous ones remain retained.
- Output capacity stays in frame->size; valid decoded bytes are
  `3 + frame->raw[2]`, excluding flags/checksum. No data copy is required for
  delivery. Retained output storage must not be supplied again until released
  by its consumer.
- A null carrier returned by acquire violates the supply contract and asserts;
  it is not a resource-availability query or a silent frame-drop branch.
  Length exceeding the supplied capacity is a receive failure, not an assertion:
  clear progress and wait in existing idle for a new flag.
  Oversized frames retain their carrier for reuse after resynchronization.
- Decoder assertions are concentrated at constructor/dispatch input boundaries,
  newly acquired carrier validation, the append write bound, and the switch
  default for invalid states. Expand cold-path constructor checks individually;
  dispatch checks me, e, and the signal separately. Keep acquired carrier checks
  combined using short-circuit &&, with null checks before dependent dereferences.
  Do not repeat stable interface/carrier checks or
  cursor/count facts already established by state transitions. Length capacity
  rejection remains a normal input guard. Checksum is not stored, so a decoded
  frame can occupy the carrier's entire capacity without another write.
- Serialize operations on each instance. A decoder callback must not dispatch
  or reconstruct the same decoder. It may drive a different instance.
  `HDLC_ASSERT` detects contract violations. The default maps to standard C
  `assert`; `NDEBUG` removes default checks, not the caller's obligations.
- Different instances have independent state. There is no internal locking;
  synchronization for shared output buffers or callback contexts is external.
  There is no runtime reentrancy guard; serialization remains the caller's
  responsibility.
- Encoder construction mounts a framing vtable and an optional instance-level
  `HdlcEncoderNotifySink` value containing notify/ctx, and starts in dead with no
  frame mounted. The interface value is copied into the instance; a null notify
  function disables notification, and its ctx is then unused. A configured
  callback's context remains valid for subsequent notifications.
  It owns frame encoding and reclamation, not sequence management, a queue,
  a device, interrupts, polling, or DMA state.
- The encoder exposes only `HdlcEncoder_ctor`, `HdlcEncoder_init`, and
  `HdlcEncoder_dispatch`. Completion queries, synchronous driving loops,
  standalone byte encoding, and cancellation are not encoder APIs.
- `HdlcEncoder_init(me, sink)` accepts a `HdlcEncoderFrameSink` only when no frame
  is outstanding. It mounts the descriptor, clears `idx` and `chksum`, enters
  idle, and notifies `HDLC_ENCODER_BUSY` without emitting bytes. It replaces the
  former separate-field begin API. Init cannot overwrite an outstanding frame.
- Encoder ctor checks remain individually expanded on the cold path. Init
  checks me and the dead-state precondition separately, then validates the
  submitted descriptor with one short-circuit assertion. Dead already implies
  no mounted frame, so that fact is not checked again. Dispatch checks only me;
  the switch default catches invalid states. Keep the raw read-index bound and
  one combined framing-transform result assertion at their operation boundaries.
- The per-frame descriptor has `raw` and two embedded interface values:
  `HdlcEncoderByteSink emit` contains emit/ctx, and `HdlcEncoderGcSink gc`
  contains gc/ctx. Raw and both callbacks are required; contexts may be null if
  callbacks permit. The encoder still borrows the enclosing frame descriptor;
  aggregating endpoints does not change its lifetime or copy it into the encoder.
  `raw` points at `Seq RecID Length Payload`, with no flags or required checksum.
  At least `3 + raw[2]` bytes must be readable. That capacity is a caller
  obligation because the descriptor carries no independent buffer size.
  Sequence, RecID, and length are read from raw, not copied into instance fields.
- Acceptance transfers the input carrier's processing and reclamation lifetime
  to the encoder. The descriptor and raw bytes must remain valid and immutable
  until GC. Allocation, pools, and reference counting remain application-owned.
  Static carriers may supply a no-op GC callback. The descriptor itself may
  reside in the allocation reclaimed by GC.
- `HdlcEncoder_dispatch(me)` advances encoding directly, without an event object.
  The first dispatch from idle emits the opening flag. Each encoding dispatch
  emits exactly one wire byte; transformed bytes require a separate dispatch.
  The closing flag enters inactive without releasing or notifying.
- The next dispatch from inactive confirms completion of the last output:
  detach the descriptor, GC exactly once, notify `HDLC_ENCODER_READY`, then enter
  dead. This follows the accepted model's GC/notify-before-dead action order.
  The application supplies the completion fact; the encoder does not inspect
  hardware. Dead consumes extra dispatches without output, GC, or notification.
- The owner records BUSY/READY through notify and controls any polling or
  interrupt-driven progression. READY is an action notification, not permission
  to reenter the same instance synchronously from a callback.
- GC receives its context and the original raw pointer. The instance's frame
  reference is cleared before GC; no descriptor or raw access is allowed after
  it. Notify and its context are instance-level and must survive frame GC.
- After init accepts a frame, drive it through completion before reclaiming or
  reusing its descriptor and raw storage. No cancellation path is exposed.
- An owner may implement synchronous driving by dispatching until notify reports
  READY, including the final no-output completion step. Use that approach only
  when emit's return establishes output completion, such as writing to an
  adequately sized buffer. For asynchronous hardware, the application dispatches
  at the appropriate readiness/completion points. Encoder READY for a buffer
  does not mean DMA has sent it; buffer reclamation is a separate lifetime.
- Length is 0..255. Even zero payload requires a readable three-byte header and
  produces a complete header, checksum, and pair of flags.
- The sink must accept every emitted byte in order. The application steps
  only when the sink can accept a byte; there is no sink rejection or retry.
  A buffer sink must have sufficient capacity before driving the FSM.
  No dedicated buffer/DMA interface is provided in this phase.
- The sink context may be null if the sink permits it. Its lifetime must cover
  the encoder's use. A callback must not init, dispatch, reconstruct, or mutate
  the same encoder. Different encoder instances can be driven from it.
- Construct before init. Complete init before enabling or kicking interrupt
  progression; serialize init, dispatch, and all same-instance callbacks.
- Applications capturing a frame need at most `2 + 2 * (4 + payloadSize)`
  wire bytes, at most 520 bytes for the maximum payload.
- The encoder has no transport dependency; delivery to the sink is not a
  guarantee of physical transmission.

## Framing polymorphism

- Both constructors require a non-null `HdlcFramingVtbl const *`. Pass
  `&HdlcFraming_defaultVtbl` for the original flag, escape, and XOR rules.
- The table supplies `frameFlag()`, `escape()`, and `transform(byte)`.
  All entries are required. Flag and escape must be distinct bytes.
- Each instance borrows its table; the table must remain valid and immutable
  for that instance's lifetime. No mid-frame rebinding or global mutable rules.
  Different instances may use different tables, or share the same immutable
  table.
- All methods must be pure, deterministic, and must not reenter or mutate
  the caller. There is no additional policy context or allocation.
- Transform has the mounted instance's direction: an encoder maps one
  flag/escape byte to one data byte following the escape prefix; the result must
  be neither flag nor escape. A decoder reverses the matching encoder's mapping
  for both protected bytes and defines its transform for every byte supplied
  after an escape. Non-symmetric rules require separate TX/RX tables with
  matching flag/escape values and inverse transforms. The default XOR transform
  is self-inverse, so its table can be shared by both directions.
- The field layout and one-byte additive checksum are fixed on both sides;
  the decoder's FSM topology is unchanged. Only flag, escape, and byte
  transformation are polymorphic.
- `tests/hdlc_test_frame.h` independently constructs reference frame bodies
  for comparison against encoder output in compatibility tests.
- `hdlc_framing_test` verifies separate TX/RX tables with custom flag/escape and
  inverse arithmetic transforms alongside the shared default XOR table using
  interleaved instances.

## Port and application routing

- All encoder/decoder assertions use `HDLC_ASSERT(expr)` from
  `ports/hdlc_port.h`. CMake adds `ports/` as a private library include path;
  non-CMake builds must add it explicitly.
- Define `HDLC_PORT_HEADER` to a quoted application header name when compiling
  each library, and add that header's directory to its include paths. The
  application header defines `HDLC_ASSERT(expr)`. Alternatively predefine
  `HDLC_ASSERT` directly. No custom definition means standard C `assert`.
- A custom assertion must evaluate its condition once and must not return on
  failure. This is a compile-time module port, not a per-instance callback.
  A custom implementation controls its own behavior under `NDEBUG`.
- Application code classifies raw bytes, chooses the decoder instance, and
  calls `HdlcDecoder_dispatch(me, &event)`. The former raw-byte `input` API
  has been removed.
- `HdlcDecoderEvt` carries `sig` and `byte`. Applications map the decoder's
  `framing->frameFlag()` to `HDLC_DECODER_FRAME_FLAG_SIG`,
  `framing->escape()` to `HDLC_DECODER_ESCAPE_SIG`, and
  all other bytes to `HDLC_DECODER_BYTE_SIG`, preserving the original byte.
- The FSM trusts the signal, not the byte value. Control signals ignore `byte`;
  BYTE uses it as data, even if it equals `0x7E` or `0x7D`. Escape decoding
  remains part of the FSM. Unknown signals violate the dispatch contract.
- Dispatch consumes the event synchronously and retains no event pointer.
  The test-only application adapter is `tests/hdlc_test_router.h`.
- `hdlc_port_test` compiles both implementations with a custom assertion port
  and `NDEBUG`, verifying the override and invalid-call checks.

## Encoder model

Use one state switch in dispatch, one case per state, and one normal exit.
The accepted source is
`/mnt/c/mengrendufu/workshop/qm/YOLO/code_gen/snmtt_encoder/snmtt_encoder.c`
(QM model `YOLO_qm523.qm`, machine `SNMTT_Encoder`). Do not edit generated C.
The active composite's initial transition is expanded into the opening-flag
case; the remaining 14 stable states have explicit cases. Instance execution
context is `state + idx + chksum`, plus frame and instance-level mounts.
`idx` is size_t because raw can contain 258 bytes.

Init enters idle with a frame. Dispatch advances through opening flag, Seq,
RecID, Length, Payload, Checksum, frameEnd (closing flag), inactive, and dead.
Each data field has an escaped substate for the transformed byte on a later
dispatch. Zero payload skips Payload, including when custom framing escapes
the zero Length. Each raw byte contributes to checksum exactly once before any
escape prefix; escaped substates never accumulate again. Complement the checksum
once in its ordinary state and retain it for the escaped checksum state.
The source's `_XXX_` names are abstract operations, not literal macro semantics:
ordinary payload/checksum branches emit unchanged bytes; only escape substates
apply the framing transform.

The source's payload decrement/done operations are represented by advancing idx
once per completed raw byte and comparing it to `3 + raw[2]`, avoiding a duplicate
remaining-length field. The prefix leaves idx unchanged. Both ordinary and
escaped last-payload paths select Checksum immediately after completing output.
The FSM does not inspect transport readiness or distinguish hardware and buffer
sinks. The application controls dispatch timing.
`tests/hdlc_encoder_lifecycle_test.c` checks the extra completion dispatch,
BUSY/GC/READY order, duplicate dispatches, and exact-sized owned
carriers whose descriptor and raw bytes are both reclaimed by GC.

## Decoder model

The source model is the QM-generated HSM:
`/mnt/c/mengrendufu/workshop/qm/YOLO/code_gen/snmtt_hdlc/snmtt_decoder.c`.
The confirmed addition to that topology is conditional acquire and carrier
validation in active entry. Generated C is not edited.
Analyze behavior as HSM; implement it as an explicit switch/case FSM.
Use one state switch in the public dispatch function, one case per flattened
state, and a single normal exit. Handle events and inherited parent transitions
explicitly inside each state case; do not pre-route control events, merge
normal/escaped states, or introduce a second dispatch helper.

- Top initial -> idle.
- Active entry -> acquire only if no carrier, validate, clear partial frame;
  active initial -> waitSeq.
- Active children: waitSeq, waitRecId, waitLength, waitPayload, waitChecksum.
- Each active child has an escaped substate.
- A flag from idle enters active; a flag anywhere in active re-enters active.
- Escape enters the respective escaped substate; another escape returns idle.
- A normal byte in an escaped substate is decoded by the instance's framing
  vtable before field handling (XOR for the default vtable).
- Seq -> RecID -> Length -> Payload (when nonzero) -> Checksum.
- Each payload byte decrements the remaining count; zero -> Checksum.
- Checksum emits only if valid and returns idle on either outcome. Invalid
  checksum retains the carrier. Another flag is required before the next Seq;
  a shared boundary flag or separate closing/opening flags are both supported.
  Bare length-delimited frames without an intervening flag are not accepted.
- Carrier acquisition and capacity-failure branches reuse existing states; there
  are no additional states, dispatch switches, or application event classifiers.
  `hdlc_decoder_carrier` covers retained and immediately freed carriers, exact
  capacities 3..258, active-entry acquisition, reuse after errors, and
  reconstruction. The port test checks null/invalid supply at active entry.

## Style

Match the existing C conventions: four spaces, trailing underscore on helpers,
`Object_action` public functions, `uint8_t const *` pointer qualifiers.
Do not add or remove code comments unless explicitly requested.
