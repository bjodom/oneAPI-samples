all: montecarlo

!if "$(generator)" == "mrg"
	GENERATOR=/DUSE_MRG
!endif

!if "$(generator)" == "philox"
	GENERATOR=/DUSE_PHILOX
!endif

!if "$(init_on_host)" == "1"
        INIT_ON_HOST=/DINIT_ON_HOST=1
!endif

DPCPP_OPTS=/I"$(MKLROOT)\include" /DMKL_ILP64 $(GENERATOR) /EHsc -fsycl $(INIT_ON_HOST) /Qmkl /Qmkl-sycl-impl=rng OpenCL.lib

montecarlo: src/montecarlo_main.cpp
	icx src/montecarlo_main.cpp /omontecarlo.exe $(DPCPP_OPTS)

clean:
	del /q montecarlo.exe

pseudo: clean all
