# Makefile for NMAKE

default: run

all: run

run: t_test.exe t_test_usm.exe
	.\t_test.exe
	.\t_test_usm.exe

DPCPP_OPTS=/I"$(MKLROOT)\include" /Qmkl /Qmkl-sycl-impl="stats,rng" /DMKL_ILP64 /EHsc -fsycl-device-code-split=per_kernel -fno-sycl-early-optimizations OpenCL.lib

t_test.exe: t_test.cpp
	icx-cl -fsycl t_test.cpp /Fet_test.exe $(DPCPP_OPTS)

t_test_usm.exe: t_test_usm.cpp
	icx-cl -fsycl t_test_usm.cpp /Fet_test_usm.exe $(DPCPP_OPTS)

clean:
	del /q t_test.exe t_test_usm.exe

pseudo: clean run all
