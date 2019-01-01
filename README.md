
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
- A somewhat capable compiler (gcc/clang), 
- make (GNU,BSD)
- sh (sh/bash/ksh)
- coreutils (awk,grep,sed,date,touch,head,printf,which,find,test,...)

This is not an exhaustive list but the list of systems on which it has been built:
- Linux: slitaz 4 2.6.37, ubuntu 12.04 3.11.0, debian9.
- OSX 10.11.6
- OpenBSD 5.5
- FreeBSD 11.1

## Compilation
### Cloning **vsensorsdemo** repository
**vsensorsdemo** is using SUBMODROOTDIR Makefile's feature (RECOMMANDED, see [submodules](#compilation/usinggitsubmodules)):  
    $ git clone https://github.com/vsallaberry/vsensorsdemo.git
    $ git submodule update --init # or just 'make'  

Otherwise:  
    $ git clone --recursive https://vsallaberry/vsensorsdemo.git
### Building
Just type:  
    $ make # (or 'make -j3' for SMP)  

If the Makefile cannot be parsed by 'make', try:  
    $ ./make-fallback  
### General information
An overview of Makefile rules can be displayed with:  
    $ make help  

Most of utilities used in Makefile are defined in variables and can be changed
with something like 'make SED=gsed TAR=gnutar' (or ./make-fallback SED=...)  

To See how make understood the Makefile, you can type:  
    $ make info # ( or ./make-fallback info)  

When making without version.h created (not the case for this repo), some old
bsd make can stop. Just type again '$ make' and it will be fine.  
### Using git submodules
When your project uses git submodules, it is a good idea to group
submodules in a common folder, here, 'ext'. Indeed, instead of creating a complex tree
in case the project (A) uses module B (which uses module X) and module C (which uses module X),
X will not be duplicated as all submodules will be in ext folder.  

You need to set the variable SUBMODROOTDIR in your program's Makefile to indicate 'make'
where to find submodules (will be propagated to SUBDIRS).  

As SUBDIRS in Makefile are called with SUBMODROOTDIR propagation, currently you cannot use 
'make -C <subdir>' (or make -f <subdir>/Makefile) but instead you can use 'make <subdir>',
 'make {test,test-build,install,debug,...}-<subdir>', as <subdir>, test-<subdir>, ... are
defined as targets.  

You can let SUBMODROOTDIR empty if you do not want to group submodules together.

## Contact
[vsallaberry@gmail.com]  
<https://github.com/vsallaberry/vsensorsdemo>

## License
GPLv3 or later. See LICENSE file.

CopyRight: Copyright (C) 2017-2018 Vincent Sallaberry

