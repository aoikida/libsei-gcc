A simple application to demonstrate how to use libsei.

Makefile contains a minimal example of how the application should be
compiled against libsei. The following compiler flags are required:

  `-fgnu-tm`               - to enable transactional memory support (gcc >= 4.7)
  `-I/libsei/include/path` - path to sei.h
  `-L/libsei/library/path` - path to libsei.a

  `-lsei -dl`              - the library itself and the dynamic linker library

