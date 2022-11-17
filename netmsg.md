# isi Message Exchange Protocol (IMXP)

Protocol Version: 2.0

### Conventions

 - All numeric values are Little endian integers
 - int32 - a 32 bit signed integer
 - uint32 - a 32 bit unsigned integer
 - int16 - a 16 bit signed integer
 - uint16 - a 16 bit unsigned integer
 - uint8 - an 8 bit unsigned integer (octet)
 - `cstring` - a sequence of non-zero octets, terminated by a zero octet
   - the length of any `cstring` MUST NOT exceed 255 non-zero octets.
   - if 256 non-zero octets are read, this is considered a "framing error"
   - if the end of a frame payload is reached before a `cstring` terminating zero octet,
     then this condition is considered a "framing error"
 - `...[x]` - the item or group `...` repeated `x` times (an array)
 - `{ x y z }` - a grouped sequence of `x`, `y`, and `z` in order
 - `... [*]` - zero or more of the proceeding item, array or group
 - `... [?]` - zero or one of the proceeding item, array or group
 - `0xHHHH` - The base 16 numeric value of `HHHH`
 - `x / y` - The integer result of x divided by y, (remainder ignored)
 - `len` - The payload length in bytes, if not defined otherwise.
 - `type name;` - the item with name of `name` is of type `type`, `;` separates any following items.
 - `uint64 uuid` - a 64 bit unsigned value that must remain unique for a given `classid` between all isiCPU instances in a system.
 - `uint32 id` - a 32 bit unsigned value, that must be individually unique within an isiCPU instance.
 - `uint32 classid` - a unique 32 bit unsigned value representing a category of device or special object in isiCPU
 - `int32 err` - a signed 32 bit **error code**

### Headers and Payload Format

The IMXP frame format is as follows:

 1. uint32 head (message code, flags, frame payload length)
 2. when used over UDP, these fields are present:
   - uint32 session nonce - unique non-zero value identifying the associated session
   - uint32 frame sequence - The *sender* frame number, incremented by 1 for each frame sent
 3. if the M flag is set, a two uint16 values are present:
    - `uint16 index` starts at zero and incremented by 1 for each successive frame.
    - `uint16 final` sequence number of the final frame (this is also the total minus one)
    - if `final` is zero, this is considered a "framing error".
    - if `index` is larger than final, this is considered a "framing error".
    - if `final` differs between two frames that are part of the same multi-part message
      this is considered a "framing error"
 4. if the T (transact extension) flag is specified, an opaque uint32 is present
    any non-zero value sent as part of a request is included in the reply frames.
 5. any future extension values as indicated by flags.
 6. payload
 7. padding (zeros) to align to 32 bits (only if end of frame is not aligned)
 8. An end frame word - aligned uint32: 0xFF8859EA

 - length is the frame payload length (in bytes) of data after the header and any extensions.
 - a "frame" is a minimum of a header + no extensions + 0 length frame + no padding + 4 byte tail
 - the max length of a frame is 8191 bytes (LMAX) + headers/extensions/padding/magic tail

 ##### Flags

 - "M" (multi) flag, when set, indicates that this is a partial message,
   and more frames are expected.
   When clear, this frame is the last or only frame of a message.
   frames with different message codes and/or transaction IDs MAY be interleaved
   together over the UDP/TCP connection.
   an implementation MUST support at a minimum:
    - receiving a single non-multi frame in the middle of a multi-frame message with
      a different message code. (in such a case, the non-multi frame would be processed first)
    - receiving interleaved multi-frames with a transaction ID, and non-transaction multi-frames.
    - receiving interleaved multi-frames with at least two different transaction IDs.
    - receiving interleaved multi-frames with at least two different message codes.
 - "R" (response) flag, when set, indicates a "response frame"
   when clear, this is a "request" or "informational" frame.
 - "T" (transact) flag, when set, a uint32 (transaction ID) is appended as a header extension.
   this flags marks the frame as part of a "transaction".
   - the transaction ID may be any non-zero value, a value of zero is considered a "framing error".
   - frames with the "T" and "M" flags set are considered
     separate messages when their transaction IDs are different.
   - the transaction ID must not be reused until the final response frame is received
     for UDP, all response multi-frames must also be acknowledged.
 - "A" (acknowledge) flag, when set, the receiving node will include this
   frame's sequence number in the next ack frame

```
code   = (head >> 20) & 0xfff     ;// ( 12 bits )
flags  = (head >> 13) & 0x7f      ;// (  7 bits )
length = (head      ) & 0x1fff    ;// ( 13 bits )
flag_multi = flags & 1
flag_resp = flags & 2
flag_txid = flags & 4
flag_ackn = flags & 8

 3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-------+-------+-------+-------+-----+-+-------+-------+-------+
|     Message Code      |0 0 0 A T R M|    Payload length       | Header
|---------------------------------------------------------------|
|                         Session Nonce                         | UDP only
|---------------------------------------------------------------|
|                        Frame Sequence                         | UDP only
|---------------------------------------------------------------|
|          Multi Final          |          Multi Index          | only when M flag is set
|---------------------------------------------------------------|
|                     Transact ID extension                     | only when T flag is set
|---------------------------------------------------------------|
|                  Future Extensions (Optional)                 |
|---------------------------------------------------------------|
|                           Payload                             | Payload
|                               +-------------------------------|
|                               |0 0 0 0 0  Padding  0 0 0 0 0 0|
|---------------------------------------------------------------|
|1 1 1 1 1 1 1 1 1 0 0 0 1 0 0 0 0 1 0 1 1 0 0 1 1 1 1 0 1 0 1 0| Tail
+-------+-------+-------+-------+-------+-------+-------+-------+
```

### Message Exchange Models

 - (C>S) client to server
 - (S>C) server to client
 - (R) response to client (solicited)
 - (S>S) server to server
 - Any - allowed in all modes
 - Tx - this message type MUST be transacted

### Message Codes

| code  | min length | max length | model | description / psudo format |other note
|------:|-----------:|-----------:|:-----:|:---------------------------|:------
| 0x000 |     0 |    0 |   Any    | ping / keepalive | an empty echo response is sent in reply. may be received without a session.
| 0x001 |     0 |   16 |    R     | ping / echo response | may be received and processed without a session
| 0x002 |    22 |   22 |   Any    | Session Hello<br>`uint16 protocol_major`<br>`uint16 protocol_minor`<br>`uint32 client_type`<br>`uint64 hello_nonce`<br>`uint32 session_nonce`<br>`uint8 extension_count`<br>`uint8 option_count`<br> | see [Session Hello] for details
| 0x003 |     4 |   12 |   Any    | Session Terminate<br>`int32 err`<br>`uint64 extra[?]` | Request other side to end the current session.
| 0x004 |     0 |    0 |   Any    | Request available extension codes |
| 0x005 |     0 |    0 |   Any    | Request options |
| 0x006 |     0 |    0 |   Any    | Request active extension codes |
| 0x007 |     0 |    0 |   Any    | Set options<br>`{ uint16 option; uint32 new_setting; }[*]` | Option list is sent in reply with all requested options and their (possibly unchanged) values
| 0x008 |     0 | LMAX |    R     | Extension list<br>`uint16 extension[*]` |
| 0x009 |     0 | LMAX |    R     | Option list<br>`{ uint16 option; uint32 setting; }[*]` |
| 0x00A |     0 | LMAX |    R     | Active Extension list<br>`uint16 active_extension[*]` |
| 0x00D |     0 | LMAX |   Any    | Disable Extensions<br>`uint16 extensions[*]` | the Active Extension list is sent in reply
| 0x00E |     0 | LMAX |   Any    | Enable Extensions<br>`uint16 extensions[*]` | the Active Extension list is sent in reply
| 0x010 |     0 |   16 |   Any    | request ping / echo | message payload is echoed back. may be received without a session.
| 0x011 |     0 |    0 |   C>S    | request accessible objects
| 0x012 |     0 |    0 |   C>S    | request sync all objects
| 0x013 |     0 |    0 |   C>S    | request classes
| 0x014 |     0 |    0 |   C>S    | request hierarchy
| 0x020 |     4 | LMAX | Tx, C>S  | create object<br>`uint32 classid`<br>*parameter list*
| 0x021 |     4 |    4 |   C>S    | delete object<br>`uint32 id`
| 0x022 |     8 |    8 |   C>S    | attach object B to A<br>`uint32 id_A`<br>`uint32 id_B`
| 0x023 |     8 |    8 |   C>S    | deattach object from A<br>`uint32 id_A`<br>`int32 at_A`
| 0x024 |     4 |    4 |   C>S    | recursive activate and reset<br>`uint32 id`
| 0x025 |     4 |    4 |   C>S    | recursive deactivate<br>`uint32 id`
| 0x026 |    16 |   16 |   C>S    | attach object B to A via points<br>`uint32 id_A`<br>`uint32 id_B`<br>`int32 at_A`<br>`int32 at_B`
| 0x02A |    12 |   12 |Tx,C>S,S>S| load object<br>`uint32 classid`<br>`uint64 uuid`
| 0x080 |     8 | LMAX |   Any    | send object message<br>`uint32 id`<br>`uint32 msg[ (len - 4) / 4 ]`
| 0x081 |     8 | LMAX |   Any    | send channel message<br>`uint32 chanid`<br>`uint32 msg[ (len - 4) / 4 ]` | requires subscribed message exchange.
| 0x100 |     9 | LMAX |   Any    | sync memory a16_d8_8<br>`uint32 id`<br>`uint16 baseindex`<br>`{ uint8 delta_next; uint8 blocklen; uint8 data[blocklen] }[*]`
| 0x101 |    11 | LMAX |   Any    | sync memory a32_d8_8<br>`uint32 id`<br>`uint32 baseindex`<br>`{ uint8 delta_next; uint8 blocklen; uint8 data[blocklen] }[*]`
| 0x102 |     8 | LMAX |   Any    | sync memory a16_8<br>`uint32 id`<br>`{ uint16 baseindex; uint8 blocklen; uint8 data[blocklen] }[*]`
| 0x103 |     9 | LMAX |   Any    | sync memory a24_8<br>`uint32 id`<br>`{ uint24 baseindex; uint8 blocklen; uint8 data[blocklen] }[*]`
| 0x104 |    10 | LMAX |   Any    | sync memory a32_8<br>`uint32 id`<br>`{ uint32 baseindex; uint8 blocklen; uint8 data[blocklen] }[*]`
| 0x110 |     5 | LMAX |   Any    | sync object run state       <br>`uint32 id`<br>`uint8 data[len - 4]`
| 0x111 |     5 | LMAX |   Any    | sync object session state   <br>`uint32 id`<br>`uint8 data[len - 4]`
| 0x112 |     5 | LMAX |   Any    | sync non-volatile state     <br>`uint32 id`<br>`uint8 data[len - 4]`
| 0x114 |     9 | LMAX |   Any    | sync object run state       <br>`uint32 id`<br>`uint32 byteoffset`<br>`uint8 data[len - 8]`
| 0x115 |     9 | LMAX |   Any    | sync object session state   <br>`uint32 id`<br>`uint32 byteoffset`<br>`uint8 data[len - 8]`
| 0x116 |     9 | LMAX |   Any    | sync non-volatile state     <br>`uint32 id`<br>`uint32 byteoffset`<br>`uint8 data[len - 8]`
| 0x140 |     8 |   12 | S>S, C>S | request object run state    <br>`uint32 id`<br>`uint32 reqlen`<br>`uint32 offset (optional)` | if offset specified, requests reqlen bytes of rvstate starting at offset byte.
| 0x141 |     8 |   12 | S>S, C>S | request object session state<br>`uint32 id`<br>`uint32 reqlen`<br>`uint32 offset (optional)` | if offset specified, requests reqlen bytes of svstate starting at offset byte.
| 0x142 |     8 |   12 | S>S, C>S | request non-volatile state  <br>`uint32 id`<br>`uint32 reqlen`<br>`uint32 offset (optional)` | if offset specified, requests reqlen bytes of nvstate starting at offset byte.
| 0x211 |     0 | LMAX |    R     | list of accessible objects <br>`{ uint32 id; uint32 classid; }[len / 8]` | sent with long list or continuation
| 0x213 |     0 | LMAX |    R     | list of classes <br>`{ uint32 id; uint32 flags; cstring name; cstring desc; }[*]` | sent with long list or continuation
| 0x214 |     0 | LMAX |    R     | list of hierarchy <br>`{ uint32 id; uint32 up_id; uint32 mem_id; }[len / 16]` | sent with long list or continuation
| 0x220 |     8 |   12 | S>C,  R  | object created<br>`int32 err (if R is set)`<br>`uint32 id`<br>`uint32 classid`
| 0x221 |     4 |    4 | S>C,  R  | object deleted<br>`uint32 id`
| 0x222 |    20 |   20 | S>C,  R  | object attach<br>`int32 err (if R is set)`<br>`uint32 id_a`<br>`uint32 id_b`<br>`int32 point_a`<br>`int32 point_b`
| 0x224 |     4 |    8 | S>C,  R  | object hierarchy started or reset<br>`int32 err (if R is set)`<br>`uint32 id`
| 0x225 |     4 |    8 | S>C,  R  | object hierarchy stopped<br>`int32 err (if R is set)`<br>`uint32 id`
| 0x22A |    20 |   20 |   Tx R   | object loaded<br>`int32 err`<br>`uint32 id`<br>`uint32 classid`<br>`uint64 uuid` | if uuid is zero or not equal to the requested uuid<br>and id is non-zero,<br>then the object loaded is a copy of the requested object.

### Framing Error

Any condition that indicates a framing error MUST be handled as so:
 - all outstanding transactions are cancelled
 - the end that detected the framing error must send session terminate
 - TCP sessions must close the TCP connection.

### Session Hello

An IMXP session **MUST** be initialized with a *session hello* message before it is allowed
to send or receive other messages.

*Keep alive*, *echo*, and *echo response* are exceptions, they **MAY** be sent over
uninitialized sessions, and any *echo* messages **SHOULD** be processed and replied to.

The *session hello* message is sent by one side to indicate it is establishing a session

 - `uint16 protocol_major` is the highest supported major version of the protocol.
 - `uint16 protocol_minor` is the associated highest supported minor version of the protocol.
 - `uint32 client_type` is an informational value indicating the implementation type, this value **MUST** be ignored by servers.
 - `uint64 hello_nonce` is a unique value identifying this hello message
 - `uint32 session_nonce` should be zero for the side initially sending a hello
 - `uint8 extension_count` the number of extensions supported.
 - `uint8 option_count` the number of session settings available.

At the beginning of a session, one side will send a *session hello* with a `hello_nonce`
and `session_nonce` set to zero, filling in the other fields as appropriate.

The other side **MUST** reply with a *session hello* message with its information
about version and extensions, the reply **WILL** have the same `hello_nonce` as received,
`session_nonce` will be set to zero if the server does not support the protocol version
supplied, otherwise `session_nonce` will be filled with a
non-zero unique value identifying the session.

 - The `session_nonce` negotiated in a TCP session MAY be used for any UDP messages
 - A `session_nonce` **MUST NOT** be used as an address or index into a structure.
 - A session negotiated over TCP **MUST NOT** send UDP until the initiating side does so
   first with a valid `session_nonce`.
 - The server **MAY** reject sessions with a *session terminate* message, it MUST set `extra`
   to `hello_nonce`, and set `err` appropriately. A server **SHOULD** allow at
   least 1 renegotiation attempt before rejecting a connection/hello.
 - *session hello* MAY be sent over either TCP or UDP, a session negotiated over UDP
   **CAN NOT** be later extended to TCP, but the reverse is true: a TCP session **MAY**
   be freely extended to include UDP as well.
 - a node that wishes to use TCP and UDP *MUST* connect and successfully negotiate
   a session over TCP before attempting to negotiate over UDP.

### Transacted messages

 - Have an extra `Transaction ID` value.
 - The contents of `Transaction ID` are returned in the matching response message.
 - **MAY** arrive out of order with and between other non-transacted messages.
 - **MAY** have longer internal processing time.

### Parameter Lists
 - A *Parameter list* is a list of tagged, variable sized arguments.
 - An entry consists of:
   1. A byte tag, object specific, zero indicates end of list, thus all options usually start at one.
   2. Followed by a byte length, this can be zero if used as a flag parameter.
   3. Optional data, must have the length as specified by length byte. (limit is 255 bytes)
 - Entries are concatenated into a list
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

### Attach Point Enums
 - values > 0 are normal bus attach points
 - values < 0 are special attach points
 - Attaching a *memory* type device currently ignores destination attach point, the destination should be set to 0 (zero) and source to -1 to maintain possible future compatibility.

|     Point    |   Value
|:------------:|------------
|  AT_BUS_END  |     -1
| AT_BUS_START |     -2
|   UP_DEVICE  |     -3

