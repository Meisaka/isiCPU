### isi Message Exchange Interface Protocol (IMEIP)

Protocol Version: 1.0

#### Conventions

 - All numeric values are Little endian integers
 - int32 - a 32 bit signed integer
 - uint32 - a 32 bit unsigned integer
 - int16 - a 16 bit signed integer
 - uint16 - a 16 bit unsigned integer
 - uint8 - an 8 bit unsigned integer (octet)
 - cstring - a sequence of non-zero octets, terminated by a zero octet
 - `...[x]` - the item or group `...` repeated `x` times (an array)
 - `{ x y z }` - a grouped sequence of `x`, `y`, and `z` in order
 - `... [*]` - zero or more of the preceeding item, array or group
 - `... [?]` - zero or one of the preceeding item, array or group
 - `0xHHHH` - The base 16 numeric value of `HHHH`
 - `x / y` - The integer result of x divied by y, (remainder ignored)
 - `len` - The payload length in bytes, if not defined otherwise.
 - `type name;` - the item with name of `name` is of type `type`, `;` seperates any following items.
 - `uint64 uuid` - a 64 bit unsigned value that must remain unique for a given `classid` between all isiCPU instances in a system.
 - `uint32 id` - a 32 bit unsigned value, that must be individually unique within a isiCPU instance.
 - `uint32 classid` - a unique 32 bit unsigned value representing a category of device or special object in isiCPU
 - `int32 err` - a signed 32 bit isiCPU **error code**

##### Stream Headers and Payload Format

When used over a stream protocol (TCP) the packet format is:

 - 32 bit header (message code, flags, length)
 - payload
 - padding (zeros) to align to 32 bits (only if end of message is not aligned)
 - end of message word - aligned 32 bit integer: 0xFF8859EA

 - length is message payload length (in bytes) after header and any extensions.
 - a "packet" is a minimum of a 4 byte header + 0 length message + no padding + 4 byte tail
 - the max length of a standard stream packet is 8191 bytes + headers/padding/magic tail

```
code   = (head >> 20) & 0xfff     ;// ( 12 bits )
flags  = (head >> 13) & 0x7f      ;// (  7 bits )
length = (head      ) & 0x1fff    ;// ( 13 bits )

 3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-------+-------+-------+-------+-----+-+-------+-------+-------+
|     Message Code      |    Flags    |    Payload length       | Header
|---------------------------------------------------------------|
|                  Header Extensions (Optional)                 |
|---------------------------------------------------------------|
|                           Payload                             |
|                               +-------------------------------|
|                               |0 0 0 0 0  Padding  0 0 0 0 0 0|
|---------------------------------------------------------------|
|1 1 1 1 1 1 1 1 1 0 0 0 1 0 0 0 0 1 0 1 1 0 0 1 1 1 1 0 1 0 1 0| Tail
+-------+-------+-------+-------+-------+-------+-------+-------+
```

##### Datagram Headers and Payload Format

When used over a datagram protocol (UDP or IP) the packet format is:

 - 32 bit head (message code, flags, length)
 - 32 bit session nonce - unique non-zero value identifying the associated session
 - 32 bit control sequence - The *sender* packet number, incremented by 1 for each packet sent
 - any extension values indicated by flags.
 - payload
 - padding (zeros) to align to 32 bits (only if end of message is not aligned)
 - An end of packet word - aligned 32 bit integer: 0xFF8859EA

 - length is message payload length (in bytes) after header and any extensions.
 - datagram minimum is a 12 byte header + 0 length message + no padding + 4 byte tail
 - the max length of a standard datagram packet is 8191 bytes + headers/padding/magic tail

```
code   = (head >> 20) & 0xfff     ;// ( 12 bits )
flags  = (head >> 13) & 0x7f      ;// (  7 bits )
length = (head      ) & 0x1fff    ;// ( 13 bits )

 3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-------+-------+-------+-------+-----+-+-------+-------+-------+
|     Message Code      |    Flags    |    Payload length       |  \
|---------------------------------------------------------------|  |- Header
|                         Session Nonce                         |  |
|---------------------------------------------------------------|  |
|                            Sequence                           |  |
|---------------------------------------------------------------|  |
|                  Header Extensions (Optional)                 |  /
|---------------------------------------------------------------|
|                           Payload                             | Payload
|                               +-------------------------------|
|                               |0 0 0 0 0  Padding  0 0 0 0 0 0|
|---------------------------------------------------------------|
|1 1 1 1 1 1 1 1 1 0 0 0 1 0 0 0 0 1 0 1 1 0 0 1 1 1 1 0 1 0 1 0| Tail
+-------+-------+-------+-------+-------+-------+-------+-------+
```

##### Message Exchange Models

 - (C>S) client to server
 - (S>C) server to client
 - (R>C) server response to client (solicited)
 - (S>S) server to server
 - Any - allowed in all modes
 - R>Any - response to message allowed in all modes.

##### Message Codes

| code  | min length | max length | model | description / psudo format |other note
|------:|-----------:|-----------:|:-----:|:---------------------------|:------
| 0x000 |     0 |    0 |   Any    | keepalive
| 0x001 |     0 |   16 |  R>Any   | ping / echo response | may be received and processed without a session
| 0x002 |    22 | 8100 |   Any    | Session Hello<br>`uint16 protocol_major`<br>`uint16 protocol_minor`<br>`uint32 client_type`<br>`uint64 hello_nonce`<br>`uint32 session_nonce`<br>`uint8 extension_count`<br>`uint8 option_count`<br>`uint16 extension[extension_count]`<br>`{ uint16 option; uint32 setting; }[option_count]` | see [Session Hello] for details
| 0x003 |     4 |   12 |   Any    | Session Terminate<br>`int32 err`<br>`uint64 extra[?]` | Request other side to end the current session.
| 0x010 |     0 |   16 |   Any    | ping / echo | message payload is echoed back. may be received without a session.
| 0x011 |     0 |    0 |   C>S    | request accessable objects
| 0x012 |     0 |    0 |   C>S    | request sync all objects
| 0x013 |     0 |    0 |   C>S    | request classes
| 0x014 |     0 |    0 |   C>S    | request heirarchy
| 0x015 |     4 |   12 | S>S, C>S | request non-volatile state<br>`uint32 id`<br>`uint32 offset (optional)`<br>`uint32 reqlen (optional)` | if offset specified, returns nvstate starting at offset bytes. if reqlen specified, returns upto reqlen bytes of nvstate starting at offset.
| 0x020 |     4 | 1300 |   C>S    | create object<br>`uint32 classid`<br>*parameter list*
| 0x021 |     4 |    4 |   C>S    | delete object<br>`uint32 id`
| 0x022 |     8 |    8 |   C>S    | attach object B to A<br>`uint32 id_A`<br>`uint32 id_B`
| 0x023 |     8 |    8 |   C>S    | deattach object from A<br>`uint32 id_A`<br>`int32 at_A`
| 0x024 |     4 |    4 |   C>S    | recursive activate and reset<br>`uint32 id`
| 0x025 |     4 |    4 |   C>S    | recursive deactivate<br>`uint32 id`
| 0x026 |    16 |   16 |   C>S    | attach with bus insert object B to A<br>`uint32 id_A`<br>`uint32 id_B`<br>`int32 at_A`<br>`int32 at_B`
| 0x030 |     8 | 1300 | C>S, S>S | transacted create object<br>`uint32 txid`<br>`uint32 classid`<br>*parameter list*
| 0x03A |    16 |   16 | C>S, S>S | transacted load object<br>`uint32 txid`<br>`uint32 classid`<br>`uint64 uuid`
| 0x080 |     6 | 1300 |   Any    | send object message<br>`uint32 id`<br>`uint16 msg[ (len - 4) / 2 ]`
| 0x081 |     6 | 1300 |   Any    | send channel message<br>`uint32 chanid`<br>`uint16 msg[ (len - 4) / 2 ]` | requires subscribed message exchange.
| 0x0E0 |     6 | 1300 | S>C, S>S | sync memory a16<br>`uint32 id`<br>`{`<br>`uint16 baseindex`<br>`uint16 blocklen`<br>`uint8 data[blocklen]`<br>`}[*]`
| 0x0E1 |     8 | 1300 | S>C, S>S | sync memory a32<br>`uint32 id`<br>`{`<br>`uint32 baseindex`<br>`uint16 blocklen`<br>`uint8 data[blocklen]`<br>`}[*]`
| 0x0E2 |     4 | 1300 | S>C, S>S | sync run volitile state <br>`uint32 id`<br> `uint8 data[len - 4]`
| 0x0E3 |     4 | 1300 | S>C, S>S | sync session volitile state <br>`uint32 id`<br>`uint8 data[len - 4]`
| 0x0E4 |     8 | 1300 |   Any    | sync non-volitile state <br>`uint32 id`<br>`uint32 byteoffset`<br>`uint8 data[len - 8]`
| 0x0E5 |     8 | 1300 |   Any    | sync run volitile state <br>`uint32 id`<br>`uint32 byteoffset`<br>`uint8 data[len - 8]`
| 0x0E6 |     8 | 1300 |   Any    | sync session volitile state <br>`uint32 id`<br>`uint32 byteoffset`<br>`uint8 data[len - 8]`
| 0x200 |  1300 | 1300 |   R>C    | list accessable objects <br>`{ uint32 id; uint32 classid; }[len / 8]` | sent with long list or continuation
| 0x201 |     0 | 1300 |   R>C    | last of list accessable objects <br>`{ uint32 id; uint32 classid; }[len / 8]` | sent at/with end of list
| 0x213 |  1300 | 1300 |   R>C    | list classes <br>`{ uint32 id; uint32 flags; cstring name; cstring desc; }[*]` | sent with long list or continuation
| 0x313 |     0 | 1300 |   R>C    | last of list classes<br>(same struct as response classes) | sent at/with end of list
| 0x214 |  1300 | 1300 |   R>C    | list heirarchy <br>`{ uint32 id; uint32 down_id; uint32 up_id; uint32 mem_id; }[len / 16]` | sent with long list or continuation
| 0x314 |     0 | 1300 |   R>C    | last of list heirarchy<br>(same struct as response heirarchy) | sent at/with end of list
| 0x220 |     8 |   12 | S>C, R>C | object created<br>`uint32 id`<br>`uint32 classid`<br>`int32 err (creating session only)`
| 0x221 |     4 |    4 | S>C, R>C | object deleted<br>`uint32 id`
| 0x222 |    20 |   20 | S>C, R>C | object attach<br>`uint32 id_a`<br>`int32 err`<br>`uint32 id_b`<br>`int32 point_a`<br>`int32 point_b`
| 0x224 |     4 |    8 | S>C, R>C | object heirarchy started or reset<br>`uint32 id`<br>`int32 err (initiating session only)`
| 0x225 |     4 |    8 | S>C, R>C | object heirarchy stopped<br>`uint32 id`<br>`int32 err (initiating session only)`
| 0x230 |    16 |   16 | R>C, R>S | transaction object created<br>`uint32 txid`<br>`uint32 id`<br>`uint32 classid`<br>`int32 err`
| 0x23A |    24 |   24 | R>C, R>S | transaction object loaded<br>`uint32 txid`<br>`int32 err`<br>`uint32 id`<br>`uint32 classid`<br>`uint64 uuid` | if uuid is zero or not equal to the requested uuid<br>and id is non-zero,<br>then the object loaded is a copy of the requested object.

##### Session Hello

An IMEIP session MUST be initialized with a *session hello* message before it is allowed
to send or receive other messages.

*Keep alive*, *echo*, and *echo response* are exceptions, they MAY be sent over
uninitialized sessions, and any *echo* messages SHOULD be processed and replied to.

The Hello message is sent by one side to indicate it is establishing a session

 - protocol_major is the highest supported major version of the protocol.
 - protocol_minor is the associated highest supported minor version of the protocol.
 - client_nonce is a unique value identifying this hello message
 - session_nonce should be zero for the side initially sending a hello
 - client_type is an informational value indicating the implementation type, this value MUST be ignored by servers.
 - extension_count/extensions should be filled with any extension codes supported.
 - option_count/option struct should be filled with any session settings requested.

At the beginning of a session one side will send a session hello with a client_nonce
and session_nonce set to zero, filling in the other fields as apropriate.

The other side MUST reply with a session hello message with its information
about version and extensions, the reply WILL have the same client_nonce as received,
session_nonce will be set to zero if the server does not support an extension or
the protocol version supplied, otherwise session_nonce will be filled with a
non-zero unique value identifying the session.

 - The session_nonce negotiated in a TCP session MAY be used for any UDP messages
 - A session_nonce MUST NOT be any address or index to a structure.
 - A session negotiated over TCP MUST NOT send UDP until the initiating side does so
   first with a valid session_nonce.
 - The server MAY reject sessions with a *session terminate* message, it MUST set `extra`
   to `client_nonce`, and set err apropriately. A server SHOULD allow at
   least 1 renegotiation attempt before rejecting a connection/hello.
 - *session hello* MAY be sent over either TCP or UDP, a session negotiated over UDP
   **CAN NOT** be later extended to TCP, but the reverse is true: a TCP session MAY
   be freely extended to include UDP as well.

##### Transacted messages

 - Have an extra "txid" value.
 - the contents of *txid* are returned in the response message.
 - may arrive out of order and between other "serialized" messages.
 - may have longer internal processing time.

##### Parameter Lists
 - A "Parameter" list is a list of tagged, variable sized arguments.
 - An entry consists of:
   1. A byte tag, object specific, zero indicates end of list, thus all options usually start at one.
   2. followed by a byte length, this can be zero if used as a flag parameter.
   3. Optional data, must have the length as specified by length byte. (limit is 255 bytes)
 - Entries are concatinated into a list
 - list ends with a zero byte or message length boundary.
 - Tag numbers need not be in any order, and objects MUST NOT require a specific order.
 - Tags are object specific.
 - Numbers stored in data, are little endian, the upper bytes are assumed to be zero if truncated.
 - There is no restriction or requirement for min/max length,
   but parameters should not contain excess data.

|  Device  | Tag |  Type  | Use
|:--------:|----:|--------|-------------------------------
|   rom    |   1 | uint32 | ROM size, overrides image size, this can truncate a loaded image.
|   rom    |   2 | uint64 | Media ID, ROM is sized to match if no size is specified.
|   disk   |   1 | uint64 | Disk Media ID, media to load from.
|   dcpu   |   1 | uint32 | cycle rate in Hz.
|   dcpu   |   2 |  none  | (flag) put DCPU in debug mode if specified.

##### Attach Point Enums
 - values > 0 are normal bus attach points
 - values < 0 are special attach points
 - Attaching a "memory" type device currently ignores destination attach point, the destination should be set to 0 (zero) and source to -1 to maintain possible future compatibility.

|     Point    |   Value
|:------------:|------------
|  AT_BUS_END  |     -1
| AT_BUS_START |     -2
|   UP_DEVICE  |     -3

