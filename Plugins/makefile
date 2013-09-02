#
# This is a Watcom makefile to create the WaWE plugins
#

# List of plugins. All of these have an enty in this file.
Plugins= EWave IRaw IMMIO PClipbrd PVolume XRaw XMMIO OGG PPlay XWAV MPA Props


all: CreatingStartHeader $(Plugins) CreatingStopHeader .SYMBOLIC

clean: CleaningStartHeader DelBinaries $(Plugins) CleaningStopHeader .SYMBOLIC

CleaningStartHeader: .SYMBOLIC
    @echo -===================================================================-
    @echo                            Cleaning plugins
    @echo -===================================================================-
    @set PluginMakeParm=clean

DelBinaries: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                            Binaries in Plugins folder
    @echo ---------------------------------------------------------------------
    @if exist *.dll @del *.dll

CleaningStopHeader: .SYMBOLIC
    @echo -===================================================================-
    @echo                     All plugins have been cleaned
    @echo -===================================================================-

CreatingStartHeader: .SYMBOLIC
    @echo -===================================================================-
    @echo                             Making plugins
    @echo -===================================================================-
    @set PluginMakeParm=

CreatingStopHeader: .SYMBOLIC
    @echo -===================================================================-
    @echo                      All plugins have been made
    @echo -===================================================================-

EWave: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                         Plugin: EWave
    @echo ---------------------------------------------------------------------
    @cd EWave
    @wmake -h $(%PluginMakeParm)
    @cd ..

IRaw: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                          Plugin: IRaw
    @echo ---------------------------------------------------------------------
    @cd IRaw
    @wmake -h $(%PluginMakeParm)
    @cd ..

IMMIO: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                         Plugin: IMMIO
    @echo ---------------------------------------------------------------------
    @cd IMMIO
    @wmake -h $(%PluginMakeParm)
    @cd ..

PClipbrd: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                      Plugin: PClipbrd
    @echo ---------------------------------------------------------------------
    @cd PClipbrd
    @wmake -h $(%PluginMakeParm)
    @cd ..

PVolume: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                       Plugin: PVolume
    @echo ---------------------------------------------------------------------
    @cd PVolume
    @wmake -h $(%PluginMakeParm)
    @cd ..

XRaw: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                          Plugin: XRaw
    @echo ---------------------------------------------------------------------
    @cd XRaw
    @wmake -h $(%PluginMakeParm)
    @cd ..

XMMIO: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                         Plugin: XMMIO
    @echo ---------------------------------------------------------------------
    @cd XMMIO
    @wmake -h $(%PluginMakeParm)
    @cd ..

OGG: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                 Plugin: XOGG and IOGG
    @echo ---------------------------------------------------------------------
    @cd OGG
    @wmake -h $(%PluginMakeParm)
    @cd ..

PPlay: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                         Plugin: PPlay
    @echo ---------------------------------------------------------------------
    @cd PPlay
    @wmake -h $(%PluginMakeParm)
    @cd ..

XWAV: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                          Plugin: XWAV
    @echo ---------------------------------------------------------------------
    @cd XWAV
    @wmake -h $(%PluginMakeParm)
    @cd ..

MPA: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                 Plugin: XMPA and IMPA
    @echo ---------------------------------------------------------------------
    @cd MPA
    @wmake -h $(%PluginMakeParm)
    @cd ..

Props: .SYMBOLIC
    @echo ---------------------------------------------------------------------
    @echo                                                         Plugin: Props
    @echo ---------------------------------------------------------------------
    @cd Props
    @wmake -h $(%PluginMakeParm)
    @cd ..

