Installation Instructions
=========================

Dependencies
------------
The tools have the following dependencies:

1. CMake 2.6.4 or later
2. Boost
3. ROOT

Compiling
---------
To compile, you must first generate the CMake build files.  From the source directory, do:

$ mkdir build

$ cd build

$ cmake ..

$ make

You can now run the tools directly from the build directory, e.g.

$ ./skimslim --help

