#      -- Makefile to create MPEG Audio support DLLs for WaWE --


all : lamecore impa xmpa .SYMBOLIC

lamecore : .SYMBOLIC
	@wmake -h -f makefile.lamecore

impa : .SYMBOLIC
	@wmake -h -f makefile.impa

xmpa : .SYMBOLIC
	@wmake -h -f makefile.xmpa

clean : .SYMBOLIC
        @if exist *.dll @del *.dll
        @if exist *.obj @del *.obj
        @if exist *.map @del *.map
        @if exist *.sym @del *.sym
        @if exist *.res @del *.res
        @if exist *.lst @del *.lst
