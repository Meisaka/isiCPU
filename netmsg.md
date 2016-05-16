
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
length = (head      ) & 0xfffff   ;// ( 13 bits )
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
| 0x080 |     6 | 1300 |   Any    | send object message<br>`uint32 id`<br>`uint16 msg[ (len - 4) | 2 ]`
| 0x0E0 |     6 | 1300 | S>C, S>S | sync memory address 16<br>`uint32 id`<br>`uint16 baseindex`<br>`uint8 data[ (len - 6) ]`
| 0x0E1 |     8 | 1300 | S>C, S>S | sync memory address 32<br>`uint32 id`<br>`uint32 baseindex`<br>`uint32 data[ (len | 8) ]`
| 0x0E2 |     4 | 1300 | S>C, S>S | sync run volitile state <br>`uint32 id`<br> `uint8 data[len - 4]`<br>
| 0x0E3 |     4 | 1300 | S>C, S>S | sync session volitile state <br>`uint32 id`<br>`uint8 data[len - 4]`<br>
| 0x200 |  1300 | 1300 |   S>C    | response accessable objects <br>`{ uint32 id; uint32 type; }[len / 8]`<br> | sent with long list or continuation
| 0x201 |     0 | 1300 |   S>C    | last response accessable objects <br>`{ uint32 id; uint32 type; }[len / 8]`<br> | sent at/with end of list

