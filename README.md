isiCPU
======

A network server for emulating CPUs (DCPU 1.7 specifically), and providing API and protocols to support associated hardware emulation, provisioning and management.

Actual Features
------

 - CPUs and hardware store components of state in isiCPU.
 - Fairly abstract and portable hardware and CPU interface for emulation.
 - Network access to multiple DCPU terminals (Keyboard and Display).
 - Multiple DCPUs can be run at once, and can be added at runtime.
 - Configurable TCP port.
 - supports both IPv4 and IPv6 using TCP sockets.
 - DCPU disassembly and debugger.
 - Endian swap option for bin files.
 - bin files load to a "rom" device.
 - Data persistence via external Redis database.
 - load estimation and support for multiple CPUs (mostly works)
 - single-threaded processes, multiple processes must be run on a server to use more cores.

Supported Emulations
------
 - [DCPU 1.7] Emulation
 - DCPU generic ASCII Keyboard
 - DCPU generic Clock
 - Mackapar 3.5" floppy
 - generic FlashROM/EEPROM
 - Nya Elektriska LEM 1802
 - MEI Imva
 - MEI EDC (direct connect)
 - 2 channel speaker
 - Kai Communications Hardware Interface Card (HIC)

Design Goals and Planned Features
------

 - Add various [new hardware devices][Hardware]
 - Provide a network interface to the CPU and associated hardware. (WIP)
 - Runtime debug of hardware and CPUs from network API. (WIP)
 - Emulation layer will provide an API that allows add-on shared libraries.
 - Add-on libraries will be able to be loaded during emulation.
 - Add CPU [TR3200]
 - Add CPU [DCPU-16N]
 - Add CPU [Mira2204]
 - support clustered networking.
 - Multiple processes on the same server will use Unix Domain sockets (or equivilent).
 - support using UDP sockets.
 - each instance in a cluster should allow migrating emulation state(s) to another server, both for redundancy and load sharing.
 - instances will be able to be added and removed dyamically from a cluster.

  [DCPU 1.7]: https://raw.githubusercontent.com/gatesphere/demi-16/master/docs/dcpu-specs/dcpu-1-7.txt "DCPU Specs"
  [Hardware]: https://github.com/techcompliant/TC-Specs "Tech Compliant Specs"
  [TR3200]: https://github.com/trillek-team/trillek-computer/blob/master/cpu/TR3200.md "TR3200 CPU Specs"
  [DCPU-16N]: https://gist.github.com/Meisaka/8800367 "DCPU-16N Specification"
  [Mira2204]: https://github.com/Meisaka/MiraISA/blob/mira2204/mira2204.txt "Mira2204 Instruction Set Architecture"

Programming Language
------

 - isiCPU is written in C++ using the c++11 standard or newer.
 - The CPU and Hardware API must be in C++.
 - Dynamic libraries should provide a C-style entry point (extern "C")
 - JIT (if used) may include assembly language, which should support IA32 and amd64.
 - (planned) optional GPU acceleration features?

Building
------

isiCPU was designed for GNU/Linux or compatible BSD systems.
The current build method is simple.

    make


Debugger
-------

isiCPU includes a built-in debugger that is enabled with `-D` on the command line.
Some of these features work with debugging disabled.

##### Interactive Console Commands
 - x : exit
 - *enter* : run 1 instruction
 - c : continue execution
 - bHHHH : toggle break point at HHHH (must be 4 hex char address)
 - rHHHH : read memory at HHHH (must be 4 hex char address)
 - f*num* : run for decimal num of cycles
 - t : toggle trace on execute

