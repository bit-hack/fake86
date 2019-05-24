# Fake86 Redux

__Fake86 Redux__ is a portable, open-source 8086 IBM Compatable PC-XT emulator.
Currently it is very much a work in progress and is changing rapidly.

Official Code repository: [https://github.com/8BitPimp/fake86](https://github.com/8BitPimp/fake86)

Authors:
- 2019 Aidan Dodds aka [8bitpimp](https://github.com/8bitpimp)
- 2010-2013 Mike Chambers (`miker00lz at gmail dot com`)


## Objectives

The original Fake86 seems to have been abandoned as the last update was in 2013.  Reviewing the latest code, Mike had achieved a lot with this project, however the code was messy and hard to follow in many places (It was one of his first C project remember).

When I forked the project, my objective was to make the code base as clean as possible, document the code, improve emulation speed, accuracy and compatibility.

The last available code was taken and a no-compromises policy was taken when reworking it to improve the core structure of the code for both speed and maintainability.  Large portions of code were thrown out in the process, however I believe this is necessary to unburden the core of the emulator.  In time these will be re-written and re-integrated into the project in an order that makes sense.  There were a several hacks in the code-base to get things to work, and in the course of time these are being removed and features are being added to make them entirely unnecessary.

If you diff the two codebases you will see almost every line of code has been touched in some manner.


## Features
- Intel 80186+ CPU emulation
- CGA/EGA/VGA graphics emulation
- Floppy drive and hard disk emulation
  - Disk image or device pass-through
- (_poor_) Internal PC speaker emulation
- Standard Microsoft-compatible serial mouse emulated on COM1
- Written in portable C, and runs on Linux and Windows


## Status
- Can boot any 16-bit flavor of DOS
  - MS-DOS, FreeDos, etc...
- Compatible with test roms such as Landmark
- For game compatibility [check here](COMPATIBILITY.md)


## Compiling Fake86

Requirements
- [CMake build system](https://cmake.org/)
- [SDL1.2](https://www.libsdl.org/download-1.2.php)

For linux:
```
mkdir build
cd build
cmake ..
make
```

For windows:
```
mkdir build
cd build
cmake-gui ..
```


## Similar Projects


- [DosBox](https://www.dosbox.com/)
- [8086 Tiny](https://github.com/adriancable/8086tiny)

## Links

- The old [official Fake86 homepage](http://fake86.rubbermallet.org)
- [Super PC/Turbo XT BIOS 3.1](http://www.phatcode.net/downloads.php?id=101)
- [Super Soft. Landmark](http://www.minuszerodegrees.net/supersoft_landmark/Supersoft%20Landmark%20ROM.htm)
