
## vsensorsdemo
---------------

* [Overview](#overview)
* [System Requirments](#systemrequirments)
* [Compilation](#compilation)
* [Contact](#contact)
* [License](#license)

## Overview
**vsensorsdemo** is a test program for 
libvsensors (<https://github.com/vsallaberry/libvsensors>)
and vlib (<https://github.com/vsallaberry/vlib>).

NOTE: This is a work in progress, this program is not fully operational yet.

## System requirements
- A somewhat capable compiler (gcc/clang), make (GNU,BSD), sh (sh/bash/ksh)
  and coreutils (awk,grep,sed,date,touch,head,printf,which,find,test,...)

This is not an exhaustive list but the list of systems on which it has been built:
- Linux: slitaz 4 2.6.37, ubuntu 12.04 3.11.0, debian9.
- OSX 10.11.6
- OpenBSD 5.5
- FreeBSD 11.1

## Compilation
vsensorsdemo use SUBMODROOTDIR Makefile feature which can group together all
submodules (including submodules of submodules) in one folder.
Therefore, you don't need to clone the repository with recursive option, as submodules
of submodules are included already.
$ git clone https://github.com/vsallaberry/vsensorsdemo
$ git submodule update --init # or just '$ make'

Just type:  
    $ make # (or 'make -j3' for SMP)

If the Makefile cannot be parsed by 'make', try:  
    $ ./make-fallback

Most of utilities used in Makefile are defined in variables and can be changed
with something like 'make SED=gsed TAR=gnutar' (or ./make-fallback SED=...)

To See how make understood the Makefile, you can type:  
    $ make info # ( or ./make-fallback info)

When making without version.h created (not the case for this repo), some old
bsd make can stop. Just type again '$ make' and it will be fine.

## Contact
[vsallaberry@gmail.com]  
<https://github.com/vsallaberry/vsensorsdemo>

## License
GPLv3 or later. See LICENSE file.

CopyRight: Copyright (C) 2017-2018 Vincent Sallaberry

