# AV source wire protocol

Binary TCP protocol to stream audio and video data from a device to a client.
The device is the server: it listens on port 3333 and serves one client at a
time. The client drives the exchange, asking for media a frame at a time.

The service is advertised over mDNS with service type
`_wendy_lite_av_source._tcp`.

The current protocol version is 1.1.

## Message framing

Everything is exchanged as messages: a 4 byte header followed by a payload,
every multi-byte field in network order (big-endian).

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | magic, always `0xAF` |
| 1 | 1 | message type |
| 2 | 2 | payload size |

A message is at most 1408 bytes including the header, so a payload is at most
1404 bytes.

Messages are atomic: a sender writes one message completely before starting the
next, and never interleaves two. Messages of different types may follow each
other in any order, so a handshake answer can land between two chunks of a
frame, but never inside one.

## Message types

| Type | Name | Direction |
| --- | --- | --- |
| 0 | handshake | client to device, answered by the device |
| 1 | reserved | |
| 2 | command | client to device, answered by the device |
| 3 | request | client to device |
| 4 | data | device to client |

Type 1 is reserved for future use. A receiver ignores messages of an unknown
type, using the payload size to skip over them.

## Type 0 — handshake

Announces a protocol version. 4 byte payload.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | version, major |
| 2 | 2 | version, minor |

The device answers a handshake with the same message carrying its own version,
and never sends one on its own.

A device that receives a major version higher than its own closes the
connection, without answering. No other version check is part of the protocol:
what a client does with the version it reads back is up to the client.

The handshake is optional. A client that never sends one is served normally.

## Type 2 — command

Carries a small control exchange, aside from the media stream. 8 byte payload
header followed by a command body.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | request id |
| 4 | 2 | chunk and flags |
| 6 | 2 | command id |
| 8 | … | command body |

The chunk and flags field holds, from the most significant bit:

| Bits | Field |
| --- | --- |
| 15 | 0 = event, no answer expected; 1 = command, an answer is expected |
| 14 | 0 = request, 1 = answer |
| 13 | error, only meaningful in an answer |
| 12 | last_chunk |
| 11–0 | chunk number |

The two top bits give the category of the message:

| Bit 15 | Bit 14 | Category |
| --- | --- | --- |
| 0 | 0 | event, no answer |
| 1 | 0 | request, answered |
| 1 | 1 | answer |
| 0 | 1 | reserved, ignored |

The encoding carries a command in either direction, and this version only
defines the client sending requests and the device answering them. Events, and
commands sent by the device, are reserved for a future version: a peer does not
emit them, and ignores a command whose category it does not handle.

The request id is chosen by the sender and opaque to the receiver, which only
repeats it. It has nothing to do with the request id of a type 3 message; the
two live in separate spaces.

An answer repeats the request id and the command id of the request it answers,
with bit 14 set, and keeps bit 15 set. Several requests may be outstanding, and
answers may come back in any order, so a sender matches them on the request id.

The error bit is sent as zero in a request or an event. In an answer it says the
command was not carried out, either because the command id is unknown or because
it is known and failed. Such an answer carries an empty body, and a body sent
anyway is ignored. There is no error code: the protocol says only that it
failed.

The chunk number and last_chunk are there so that a later version can send a
body as a run of chunks. This version defines a single chunk only: a request and
an answer alike are one message, carrying the chunk number 0 with last_chunk
set. A receiver ignores a command message that carries another chunk number, or
one without last_chunk.

The size of the body is the payload size minus 8, so a body is at most 1396
bytes. An empty body — a ping, an error answer — is a chunk with no body bytes
at all. A command message with a payload shorter than 8 bytes is ignored, like a
message of an unknown type.

A device that has more command requests outstanding than it can hold discards
the extra ones, and the protocol has no message to report that, so a client has
to tolerate a request that is never answered.

| Command | Name |
| --- | --- |
| 0 | ping |
| 1 | channel enumeration |

All other command ids are unassigned.

### Command 0 — ping

Empty body, in the request and in the answer. Checks that the device is alive
and measures the round trip.

### Command 1 — channel enumeration

Asks for the channels the device serves. The request body is empty.

The answer body is a list of elements, one per channel variant, in ascending
channel order and, within a channel, in ascending variant order. The number of
elements follows from the size of the body.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | channel number |
| 1 | 1 | variant |
| 2 | 1 | channel category |
| 3 | 1 | size of the properties that follow |
| 4 | … | properties |

A variant is one rendition of the source a channel carries: the same thing in
another media type, another frame size or another quality. The category and the
properties of two variants of a channel may therefore differ.

This version defines the variant 0 only, so a device sends exactly one element
per channel and that element carries the variant 0. Selecting which variant a
channel serves is left to a command reserved for a future version. A type 3
request and a type 4 data message address the channel alone and carry whatever
variant the channel serves, which in this version is always the variant 0. A
client leaves alone an element whose variant is not 0, since this version gives
it no way to ask for that variant.

The property size does not count the four bytes above it. A reader uses it to
skip properties it does not know, so a later version may append properties to a
category without breaking an older reader.

The properties of an element are packed one after the other, with no alignment
of their own. Only the block they form is padded, up to the next 32 bit
boundary, so that the next element starts aligned — which holds on the wire,
since the body itself starts 12 bytes into the message. That padding is not
counted in the property size: a size of 14 is followed by 2 bytes of padding,
and a reader skips the size rounded up to a multiple of 4 to reach the next
element. A size already a multiple of 4 is followed by no padding. Padding is
sent as zero and ignored on receipt.

Channel categories:

| Value | Category |
| --- | --- |
| 0 | time |
| 1 | video |
| 2 | audio |

Properties are positional, not tagged: a category defines its properties in a
fixed order, and a later version only appends to it. Every category starts with
the media type.

| Offset | Size | Property |
| --- | --- | --- |
| 0 | 4 | media type, a four character code |

A video channel adds the frame size, so its properties are 8 bytes.

| Offset | Size | Property |
| --- | --- | --- |
| 4 | 2 | width in pixels |
| 6 | 2 | height in pixels |

A time or audio channel defines nothing beyond the media type yet, so its
properties are 4 bytes.

| Media type | Medium |
| --- | --- |
| `MJPG` | one JPEG image per frame |

All other media types are unassigned. A client leaves alone a channel whose
category or media type it does not know.

## Type 3 — request

Asks for media on one channel. 12 byte payload.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | channel |
| 1 | 1 | number of frames |
| 2 | 2 | reserved |
| 4 | 4 | delay in microseconds since the last frame |
| 8 | 4 | request id, echoed back in every chunk of the frame |

The number of frames and the delay are not honored yet: one request yields
exactly one frame, whatever they hold.

Reserved bytes are sent as zero and ignored on receipt.

Requests are served in the order they arrive. A device that has more requests
outstanding than it can hold discards the extra ones, and the protocol has no
message to report that, so a client has to tolerate a request that is never
answered.

## Type 4 — data

Carries one chunk of a frame. 20 byte payload header followed by media data.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | channel |
| 1 | 3 | last_chunk flag (1 bit) then chunk number (23 bits) |
| 4 | 4 | frame number |
| 8 | 4 | frame timestamp in microseconds |
| 12 | 4 | host timestamp in microseconds |
| 16 | 4 | request id |
| 20 | … | media data |

A frame is one unit of media on its channel, sent as a run of chunks numbered
from 0, the last one carrying the last_chunk flag. Concatenating the media data
of the chunks in order reproduces the frame.

The chunks of a frame follow each other with no other data message in between,
and only one frame is in flight at a time. Every chunk repeats the frame number
and the id of the request that asked for it.

The maximum message size leaves 1384 bytes of media data per chunk.

Frame numbers start at 0 on a new connection and count the frames the device
has sent, whatever the channel.

## Channels

No channel number carries a fixed medium. Which channels a device serves, and
what each one carries, is read from the channel enumeration. A device ignores a
request on a channel it does not serve.

Channel 0 is reserved and never used.

## Timestamps

Timestamps are 32 bit microsecond counters read from a monotonic clock, so they
wrap about every 71.6 minutes.

The frame timestamp is taken on the device clock. The host timestamp is the same
instant expressed on the client clock; while no clock synchronization is in
place it simply repeats the frame timestamp.

## Connection handling

A device serves one connection at a time. Further connection attempts are
accepted only once the current one is over.

The stream is not resynchronized after a framing error, since there is no way to
find the next message boundary reliably. A receiver closes the connection when a
header does not start with `0xAF`, or when the payload size would make the
message exceed 1408 bytes.

A message of a known type carrying an unexpected payload size is ignored, like
one of an unknown type.
