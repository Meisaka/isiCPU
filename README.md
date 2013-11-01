isiCPU
======

A network server for emulating CPUs (DCPU 1.7 specifically), and providing API and protocols to support associated hardware emulation, provisioning and management.

Design Goals
------

 - Support multiple types of CPU emulation
 - [DCPU 1.7] [1]
 - DCPU variant
 - [RC3200] [2]
 - Support multiple hardware emulations.
 - Provide a network interface to the CPU and associated hardware.
 - Both hardware and CPUs should each have their own unique ID number.
 - Emulation layer will provide an API that allows add-on shared libraries.
 - Add-on libraries will be able to be loaded during emulation.
 - CPUs and hardware must be able to be added/removed without affecting others.
 - All CPUs and hardware must have their state in isiCPU
 - isiCPU will support clustered networking.
 - isiCPU is single-threaded per process, multiple processes must be able to run on a server.
 - Multiple processes on the same server will use Unix Domain sockets (or equivilent).
 - isiCPU will support both IPv4 and IPv6 using TCP sockets.
 - each instance of isiCPU in a cluster will allow migrating emulation state(s) to another server, both for redundancy and load sharing.
 - instances will be able to be added and removed dyamically from a cluster.

  [1]: http://dcpu.com/ "DcPU Specs"
  [2]: http://github.com/Zardoz89/Trillek-Computer/blob/RC3200/RC3200.md "RC3200 (WIP) Specs"

Programming Language
------

 - isiCPU is written in C, and C++ is allowed internal to isiCPU.
 - The CPU and Hardware API must be in C.
 - JIT (if used) may include assembly language that will be called from C.
 - GLSL using OpenGL 4.4 is allowed for optional GPU acceleration features.
 - GPU acceleration is an optional feature, all functions must run in C or C++ first.

Building
------

isiCPU was designed for GNU/Linux systems, Solaris will be added as well.
The current build method is simple.

    make

