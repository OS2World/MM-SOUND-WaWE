#                      -- Makefile to create EXE file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
exename=encoder_example
object_files=encoder_example.obj

# Create debug build or not?
# Note, that debug printf's won't be visible inside a non-debug built WaWE!
debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
#has_resource_file=defined
#rcname=xxxxx-Resources

# The result can be LXLite'd too
#create_packed_dll=defined

#-----------------------------------------------------------------------------
# The next part is somewhat general, for creation of DLL files.
#-----------------------------------------------------------------------------

!ifdef debug_build
debugflags = -d2 -dDEBUG_BUILD
!else
debugflags =
!endif

dllflags = -bd
cflags = $(debugflags) -zq -bm -bt=OS2 -5 -fpi -sg -otexan -wx

.before
    set include=$(%os2tk)\h;$(%include);..\include;..\..\libogg-1.1\include;

.extensions:
.extensions: .exe .obj .c

all : $(exename).exe

$(exename).exe: $(object_files)

.c.obj : .AUTODEPEND
    wcc386 $[* $(cflags)

.obj.exe : .AUTODEPEND
    wlink system os2v2 file $(exename).obj lib ..\..\oggcore.lib OPTION OPTION ELIMINATE OPTION MANYAUTODATA OPTION SHOWDEAD OPTION QUIET


clean : .SYMBOLIC
        @if exist *.exe del *.exe
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
