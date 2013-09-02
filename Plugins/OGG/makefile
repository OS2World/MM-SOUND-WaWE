#          -- Makefile to create OGG support DLLs for WaWE --


all : oggcore iogg xogg .SYMBOLIC

oggcore : .SYMBOLIC
	@wmake -h -f makefile.oggcore

iogg : .SYMBOLIC
	@wmake -h -f makefile.iogg

xogg : .SYMBOLIC
	@wmake -h -f makefile.xogg

clean : .SYMBOLIC
        @if exist *.dll @del *.dll
        @if exist *.obj @del *.obj
        @if exist *.map @del *.map
        @if exist *.sym @del *.sym
        @if exist *.res @del *.res
        @if exist *.lst @del *.lst
