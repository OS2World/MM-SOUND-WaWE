#
# This is a Watcom makefile to make WaWE and its plugins
#

all: debug .SYMBOLIC

debug: DebugHeader _WaWE_ _Plugins_ .SYMBOLIC

release: ReleaseHeader _WaWE_ _Plugins_ .SYMBOLIC

WaWE: DebugHeader _WaWE_ .SYMBOLIC

Plugins: DebugHeader _Plugins_ .SYMBOLIC



ReleaseHeader: .SYMBOLIC
    @echo ================ RELEASE BUILD ==========================
    @echo ======= Make sure you've "wmake clean"-ed before! =======
    @echo ================ RELEASE BUILD ==========================
    @set WaWETarget=release

DebugHeader: .SYMBOLIC
    @echo ================== DEBUG BUILD ==========================
    @set WaWETarget=debug

_WaWE_: .SYMBOLIC
    @wmake -h -f makefile.WaWE

_Plugins_: .SYMBOLIC
    @cd Plugins
    @wmake -h
    @cd ..

clean : .SYMBOLIC
    @wmake -h -f makefile.WaWE clean
    @cd Plugins
    @wmake -h clean
    @cd..
