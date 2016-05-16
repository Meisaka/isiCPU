
##### Header
 - Little endian integers
 - 32 bit header (message code, flags, length)
 - payload

| message | flags  | length
|---------|--------|--------
| 12 bits | 7 bits | 13 bits
```
code   = (head >> 20) & 0xfff     ;// ( 12 bits )
flags  = (head >> 13) & 0x7f      ;// (  7 bits )
length = (head      ) & 0x1fff   ;// ( 13 bits )
```

 - length is message length after head.
 - a "packet" is a minimum of a 4 byte head + 0 length message

##### Message types
 - (C>S) client to server
 - (S>C) server to client
 - (S>S) server to server

##### Messages
| code  | min length | max length | model | description / psudo format |other note
|------:|-----------:|-----------:|:-----:|:---------------------------|:------
| 0x000 |     0 |    0 |   Any    | keepalive
| 0x010 |     0 |   16 |   Any    | ping
| 0x011 |     0 |    0 |   C>S    | request accessable objects
| 0x012 |     0 |    0 |   C>S    | request sync all objects
| 0x013 |     0 |    0 |   C>S    | request classes
| 0x014 |     0 |    0 |   C>S    | request heirarchy
| 0x020 |     4 | 1300 |   C>S    | create object<br>`uint32 classid`<br>*parameter list*
| 0x021 |     4 |    4 |   C>S    | delete object<br>`uint32 id`
| 0x022 |     8 |    8 |   C>S    | attach object B to A<br>`uint32 id_A`<br>`uint32 id_B`
| 0x023 |     8 |    8 |   C>S    | deattach object from A<br>`uint32 id_A`<br>`int32 at_A`
| 0x024 |     4 |    4 |   C>S    | recursive activate and reset<br>`uint32 id`
| 0x025 |     4 |    4 |   C>S    | recursive deactivate<br>`uint32 id`
| 0x026 |    16 |   16 |   C>S    | attach with bus insert object B to A<br>`uint32 id_A`<br>`uint32 id_B`<br>`int32 at_A`<br>`int32 at_B`
| 0x080 |     6 | 1300 |   Any    | send object message<br>`uint32 id`<br>`uint16 msg[ (len - 4) | 2 ]`
| 0x0E0 |     6 | 1300 | S>C, S>S | sync memory address 16<br>`uint32 id`<br>`uint16 baseindex`<br>`uint8 data[ (len - 6) ]`
| 0x0E1 |     8 | 1300 | S>C, S>S | sync memory address 32<br>`uint32 id`<br>`uint32 baseindex`<br>`uint32 data[ (len | 8) ]`
| 0x0E2 |     4 | 1300 | S>C, S>S | sync run volitile state <br>`uint32 id`<br> `uint8 data[len - 4]`
| 0x0E3 |     4 | 1300 | S>C, S>S | sync session volitile state <br>`uint32 id`<br>`uint8 data[len - 4]`
| 0x200 |  1300 | 1300 |   S>C    | response accessable objects <br>`{ uint32 id; uint32 classid; }[len / 8]` | sent with long list or continuation
| 0x201 |     0 | 1300 |   S>C    | last response accessable objects <br>`{ uint32 id; uint32 classid; }[len / 8]` | sent at/with end of list
| 0x213 |  1300 | 1300 |   S>C    | response classes <br>`{ uint32 id; uint32 flags; cstring name; cstring desc; }[*]` | sent with long list or continuation
| 0x313 |     0 | 1300 |   S>C    | last response classes<br>(same struct as response classes) | sent at/with end of list
| 0x214 |  1300 | 1300 |   S>C    | response heirarchy <br>`{ uint32 id; uint32 down_id; uint32 up_id; uint32 mem_id; }[len / 16]` | sent with long list or continuation
| 0x314 |     0 | 1300 |   S>C    | last response heirarchy<br>(same struct as response heirarchy) | sent at/with end of list
| 0x220 |     8 |   12 |   S>C    | object created<br>`uint32 id`<br>`uint32 classid`<br>`int32 err (creating session only)`
| 0x221 |     4 |    4 |   S>C    | object deleted<br>`uint32 id`
| 0x222 |     8 |    8 |   S>C    | object attach response<br>`uint32 id`<br>`int32 err`
| 0x224 |     4 |    8 |   S>C    | object heirarchy started or reset<br>`uint32 id`<br>`int32 err (initiating session only)`
| 0x225 |     4 |    8 |   S>C    | object heirarchy stopped<br>`uint32 id`<br>`int32 err (initiating session only)`

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
|    Point   |   Value
|------------|------------
| AT_BUS_END |     -1
|    DOWN    |     -2

