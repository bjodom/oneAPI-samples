# Makefile for GNU Make

default: run

all: run

run: lottery lottery_usm lottery_device_api
		./lottery
		./lottery_usm
		./lottery_device_api

MKL_COPTS = -DMKL_ILP64  -qmkl -qmkl-sycl-impl=rng

DPCPP_OPTS = $(MKL_COPTS) -fsycl-device-code-split=per_kernel -fno-sycl-early-optimizations

lottery: lottery.cpp
		icpx $< -fsycl -o $@ $(DPCPP_OPTS)

lottery_usm: lottery_usm.cpp
		icpx $< -fsycl -o $@ $(DPCPP_OPTS)

lottery_device_api: lottery_device_api.cpp
		icpx $< -fsycl -o $@ $(DPCPP_OPTS)

clean:
		-rm -f lottery lottery_usm lottery_device_api

.PHONY: clean run all
