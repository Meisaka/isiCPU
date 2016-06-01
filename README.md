isiCPU
======

A network server for emulating CPUs (DCPU 1.7 specifically), and providing API and protocols to support associated hardware emulation, provisioning and management.

Actual Features
------

 - [DCPU 1.7] [1] Emulation, good accuracy and error handling
 - Keyboard and LEM are emulated and network synched.
 - CPUs and hardware store components of state in isiCPU
 - Fairly abstract and portable hardware and CPU interface for emulation.
 - Network access to a single DCPU terminal (Keyboard and LEM).
 - Configurable TCP port
 - DCPU disassembly and simple debugger
 - Endian swap option for bin files
 - bin files load to a "rom" device
 - load estimation and support for multiple CPUs (mostly works)
 - single-threaded processes, multiple processes must be run on a server to use more cores.

Design Goals and Planned Features
------

 - (Better) Support for multiple types of CPU emulation
 - Support more hardware emulations. (It's close!)
 - Add various [new hardware devices] [2]
 - Provide a network interface to the CPU and associated hardware. (WIP)
 - Both hardware and CPUs should each have their own unique ID number. (WIP)
 - CPUs and hardware must be able to be added/removed without affecting others. (WIP)
 - Runtime add/remove/configure/debug of hardware and CPUs from network API.
 - Emulation layer will provide an API that allows add-on shared libraries.
 - Add-on libraries will be able to be loaded during emulation.
 - Add CPU [TR3200] [3]
 - Add CPU [DCPU-16N] [4]
 - Add CPU [Mira2204] [5]
 - support clustered networking.
 - Multiple processes on the same server will use Unix Domain sockets (or equivilent).
 - isiCPU will support both IPv4 and IPv6 using TCP sockets.
 - each instance of isiCPU in a cluster will allow migrating emulation state(s) to another server, both for redundancy and load sharing.
 - instances will be able to be added and removed dyamically from a cluster.

  [1]: https://raw.githubusercontent.com/gatesphere/demi-16/master/docs/dcpu-specs/dcpu-1-7.txt "DcPU Specs"
  [2]: https://github.com/techcompliant/TC-Specs "Tech Compliant Specs"
  [3]: https://github.com/trillek-team/trillek-computer/blob/master/cpu/TR3200.md "TR3200 CPU Specs"
  [4]: https://gist.github.com/Meisaka/8800367 "DCPU-16N Specification"
  [5]: https://github.com/Meisaka/MiraISA/blob/mira2204/mira2204.txt "Mira2204 Instruction Set Architecture"

Programming Language
------

 - isiCPU is written in C, but it should also allow valid C++ compilation.
 - The CPU and Hardware API must be in C.
 - JIT (if used) may include assembly language that will be called from C.
 - optional GPU acceleration features.
 - GPU acceleration is an optional feature, all functions must run in C first.

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
 - <enter> : run 1 instruction
 - c : continue execution
 - bHHHH : toggle break point at HHHH (must be 4 hex char address)
 - rHHHH : read memory at HHHH (must be 4 hex char address)
 - f<num> : run for decimal num of cycles
 - t : toggle trace on execute

