INCLUDES =
LDFLAGS  = -lm
CFLAGS   = -qopenmp -fopenmp-targets=spir64 -Wall -Wformat-security -Werror=format-security
CXXFLAGS = -fsycl -Wall -Wformat-security -Werror=format-security
# Use icx from the DPC++ oneAPI toolkit to compile. Please source DPCPP's vars.sh before compilation.
CC       = mpiicx
CXX      = mpiicpx
example  = mpi3_onesided_jacobian_gpu_omp_device_initiated mpi3_onesided_jacobian_gpu_sycl_device_initiated

all: CFLAGS += -O2
all: CXXFLAGS += -O2
all: $(example)

debug: CFLAGS += -O0 -g
debug: CXXFLAGS += -O0 -g
debug: $(example)

%(example): ../include/common.h

% : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

% : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(example).o $(example)
