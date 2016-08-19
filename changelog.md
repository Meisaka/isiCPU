
0.23
------

 - added object subscribe and session as a message point
 - fix a possible issue with messaging being from the wrong source

0.22.1
------

 - changed `isi_attach` to include 2 additional (optional) parameters.
 - added fields to the "attach" response net message.
 - fix a possible issue allowing attaching the same point twice.

0.22
------

 - Added verbose cli flag, reduced verbosity by default.
 - added more entries to cli help output.
 - fix, attaching some things, incluing HIC
 - added, message channel exchange

0.21.2
------

 - transacted create object implemented

0.21.1
------

 - `deattach` implemented
 - fix a few bugs with `attach`
 - fix, m35fd now reports status changes
 - wip, adding external message forwarding "device"

0.21
------

 - new message call prototype
 - added message type constants
 - added HIC device
 - changed remaining messaging calls to use `isi_message_dev` to send messages.

0.20
------

 - Add change log
 - Additions and changes to hardware API
 - Fix disk access for filesystems.
 - wrapped memory allocation functions.
 - implemented attach with insert net message.

##### hardware API

 - isi_attach_call changed from
   `int (struct isiInfo *to, struct isiInfo *dev)`
   to `int (struct isiInfo *to, int32_t topoint, struct isiInfo *dev, int32_t frompoint)`
 - devices using QueryAttach, Attach, Attached updated.
 - attach point defines added in cputypes.h
 - Deattach added (to be implemented later)
 - structure of attach points changed
 - removed `dndev` and `hostcpu` pointers
 - all devices contain `busdev` for "down" attachments.
 - added function `isi_getindex_dev` to access attached devices.
 - rewrote `isi_attach` to use new prototype and connection logic
 - added `isi_message_dev` function to sent messages to attached devices.
 - DCPU must now be downsteam to backplane, instead of upstream.
 - DCPU hardware must sent interrupts to CPU differently and using new function.

