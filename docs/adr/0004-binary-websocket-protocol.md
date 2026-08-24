# ADR 0004 — An 8-byte binary WebSocket protocol, not JSON

- **Status:** Accepted
- **Date:** 2026-04-22, hardened 2026-05-23
- **Commits:** `a1624a9` (ProtocolHandler), `0d8598d` "full network WebSocket server firmware"

## Context

The panel needs to be drivable from a laptop — for scripted scenes, the Python
client, and LLM-generated images. The transport has to carry a full 16×16 RGB
frame at interactive rates over WiFi to a microcontroller with ~320 KB of RAM.

A 16×16 frame is 768 bytes of raw RGB. The same frame as a JSON array of triples
is roughly 4–6 KB of text that then has to be tokenised and parsed on-device.

## Decision

A **fixed 8-byte header followed by raw RGB**, over a WebSocket on port 81:

```
[0] version=1   [1] flags     [2] width    [3] height
[4] x_offset    [5] y_offset  (both int8_t — negatives allowed, for partial
                               off-screen sprites and scrolling)
[6] rotation (0-3)            [7] brightness (0-255)
payload: width * height * 3 raw RGB bytes
```

Flags: `0x01` clear before draw, `0x02` show immediately, `0x04` marquee mode,
`0x08` reply with a canvas ACK ([ADR 0007](0007-canvas-ack-readback.md)).

Parsing is a fixed-offset struct read with no allocation
(`ProtocolHandler::parseHeader`), so it is trivially unit-testable and cannot
fragment the heap.

## Consequences

**Positive.** Frames are ~6× smaller than the JSON equivalent and parse in
constant time with no dynamic memory. Sprites are a first-class concept —
arbitrary `w×h` at an offset, with rotation — rather than always full-screen.
`int8_t` offsets make scrolling and partial-visibility free.

**Negative.** Not human-readable; you cannot debug it with `curl`. The header is
fixed-size, so new capabilities must be spent from the flag bits (four of eight
are used) or bump `version`.

## Alternatives considered

**JSON over HTTP.** Rejected on payload size and parse cost, and HTTP's
request/response shape fits poorly with pushing animation frames.

**MQTT.** Rejected as it needs a broker; the point was for a laptop to talk
straight to the device.
