sm2lbpp
=======

A **S**nap**m**aker **2**.0 **L**ight**B**urn **P**ost-**P**rocessor to add thumbnails for the Snapmaker terminal.

Features
========

This application can be used as Gcode post-processing script which adds in-place thumbnail data compatible
to the Snapmaker terminal to the given file by modifying the G-Code comment sections.

Usage
=====

* Store `sm2lbpp` somewhere on your system.
* Run `sm2lbpp file.nc`.

Or, on Windows:
* Put `´scripts/register-sm2lbpp.bat` next to `sm2lbpp.exe` and run it with administrator rights.
* Right click on the `file.nc` and select `Add Snapmaker Thumbnail`.

Building
========

The following dependencies are given:  
- C99
- nanosvg
- libpng
- zlib

Edit Makefile to match your target system configuration.

_**Hint:** You may want to link with `-mwindows` for Windows targets to suppress the console window to be shown._

Building the program:  

    make

[![Linux GCC Build Status](https://img.shields.io/github/actions/workflow/status/daniel-starke/sm2lbpp/build.yml?label=Linux)](https://github.com/daniel-starke/sm2lbpp/actions/workflows/build.yml)
[![Windows Visual Studio Build Status](https://img.shields.io/appveyor/ci/danielstarke/sm2lbpp/main.svg?label=Windows)](https://ci.appveyor.com/project/danielstarke/sm2lbpp)    

Files
=====

|Name           |Meaning
|---------------|--------------------------------------------
|*.mk           |Target specific Makefile setup.
|mingw-unicode.h|Unicode enabled main() for MinGW targets.
|parser.*       |Text parsers and parser helpers.
|target.h       |Target specific functions and macros.
|tchar.*        |Functions to simplify ASCII/Unicode support.
|sm2lbpp.*      |Main application files.
|version.*      |Program version information.

License
=======

See [unlicense file](LICENSE) for this code.  
See also
[nanosvg](https://github.com/memononen/nanosvg/blob/master/LICENSE.txt),
[libpng](http://www.libpng.org/pub/png/src/libpng-LICENSE.txt) and
[zlib](https://www.zlib.net/zlib_license.html) licenses for third party code.
