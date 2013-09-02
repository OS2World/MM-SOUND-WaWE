#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=XWAV
object_files=XWAV.obj

# Create debug build or not?
# Note, that debug printf's won't be visible inside a non-debug built WaWE!
!ifeq %WaWETarget debug
debug_build=defined
!endif

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
#has_resource_file=defined
#rcname=xxxxx-Resources

#-----------------------------------------------------------------------------
# The next part is somewhat general, for creation of DLL files.
#-----------------------------------------------------------------------------

!ifdef debug_build
debugflags = -d2 -dDEBUG_BUILD
!else
debugflags =
create_packed_dll=defined
!endif

dllflags = -bd
cflags = $(debugflags) -zq -bm -bt=OS2 -5 -fpi -sg -otexan -wx

.before
    @set include=$(%os2tk)\h;$(%include);../..

.extensions:
.extensions: .dll .obj .c

all : $(dllname).dll

$(dllname).dll: $(object_files)

.c.obj : .AUTODEPEND
    wcc386 $[* $(dllflags) $(cflags)

.obj.dll : .AUTODEPEND
    wlink @$(dllname)
!ifdef has_resource_file
    rc $(rcname) $(dllname).dll
!endif
!ifdef create_packed_dll
    lxlite $(dllname).dll
!endif
    @copy *.dll ..
    @copy *.sym ..

clean : .SYMBOLIC
        @if exist *.dll @del *.dll
        @if exist *.obj @del *.obj
        @if exist *.map @del *.map
        @if exist *.sym @del *.sym
        @if exist *.res @del *.res
        @if exist *.lst @del *.lst
