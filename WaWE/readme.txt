
                         The Warped Wave Editor (WaWE)
                         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Forewords
---------

This package contains the sources of WaWE. All the sources are included in order
to be able to build WaWE and its plugins. All the sources are covered by the
LGPL license, if not noted otherwise.

This package also contains sources from other authors and other packages (for
example the ogg vorbis sources), they have their own license, of course. 
Those sources are included only for comfort, so one doesn't have to download a 
dozen of ZIP files to build WaWE, everything is included in one place.


Building WaWE
-------------

To build WaWE, you'll need the OpenWatcom compiler and the OS/2 Developer's
Toolkit installed. This means, that the makefiles assume that the INCLUDE
enviromental variable is correctly set for Watcom's include directory, and
the OS2TK variable is set to point to the root directory of the installed
OS/2 Developer's Toolkit. If all these are met, you should not have too many
problems with building WaWE.

Currently, there are two build modes of WaWE: release and debug builds.

The difference is that the debug builds will be compiled into a VIO application,
morphing into a PM application in runtime, so it will have the VIO console,
which is used to show debug messages. The release version is compiled right
into a PM application, all the printf() calls are removed, and the binaries are
lxlite'd to be smaller.

To build WaWE, all you have to do is to enter:
wmake
in the folder where you've unzipped the sources. This will build the debug
version of WaWE.exe and the plugins.

To clean the binaries (prepare for a clean new build), enter:
wmake clean
This will delete the exe, dll, obj and some other files, so the next build will
be a clean one for sure.

To build the relase version, enter:
wmake release

To build only the WaWE.exe (debug version), enter:
wmake WaWE

To build only the Plugins (debug version), enter:
wmake Plugins

It's also possible to go right into the directory of the plugin to be built, and
run wmake in there.


Missing files from this package
-------------------------------

There are some files missing from this package. They can be got by downloading
the WaWE binary release. These files are left out because they are not required
to build WaWE, and they are too big to be included everywhere.


Contacting the author
---------------------

If you have any questions, bug-reports, recommendations, ideas, or you want
to develop a plugin for WaWE, feel free to contact me at:

Doodle <doodle@scenergy.dfmk.hu>

For bug-reports, please always try the latest WaWE version, and if it's still
buggy in there, don't forget to send me the output of WaWE.exe with the
description of the problem, and the last entry of your POPUPLOG.OS2 file in the
root of your boot-drive!


<eof>

