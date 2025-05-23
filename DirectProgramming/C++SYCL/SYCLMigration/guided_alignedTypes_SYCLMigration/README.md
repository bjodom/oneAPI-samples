﻿# `Aligned Types` Sample

Data structure alignment is the way data is arranged and accessed in computer memory. Memory access is said to be aligned when the data being accessed is n bytes long and the datum address is n-byte aligned. When a memory access is not aligned, it is said to be misaligned.  The original CUDA* source code is migrated to SYCL for portability across GPUs from multiple vendors

| Area              | Description
|:---                   |:---
| What you will learn              | Migrate alignedTypes from CUDA to SYCL
| Time to complete              | 15 minutes
| Category                      | Code Optimization

## Purpose

The sample shows the migration of alignedTypes from CUDA to SYCL using SYCLomatic tool.

>**Note**: We use Intel® open-sources SYCLomatic migration tool which assists developers in porting CUDA code automatically to SYCL code. To finish the process, developers complete the rest of the coding manually and then tune to the desired level of performance for the target architecture. Users can also use Intel® DPC++ Compatibility Tool which comes along with the Intel® oneAPI Base Toolkit.

This sample contains two versions in the following folders:

| Folder Name                   | Description
|:---                           |:---
| `dpct_output`              | Contains the output of the SYCLomatic tool used to migrate SYCL-compliant code from CUDA code. The tool completely migrates code but needs manual changes to get functional correctness on the given list of hardware.
| `sycl_migrated`              | Contains the output of SYCLomatic Tool with manual changes.

## Prerequisites

| Optimized for              | Description
|:---                   |:---
| OS                    | Ubuntu* 22.04
| Hardware              | Intel® Gen9 <br> Intel® Gen11 <br> Intel® Xeon CPU <br> Intel® Data Center GPU Max <br> NVIDIA Tesla P100 <br> NVIDIA A100 <br> NVIDIA H100
| Software                | SYCLomatic (Tag - 20241104) <br> Intel® oneAPI Base Toolkit version 2025.0 <br> oneAPI for NVIDIA GPUs" plugin from Codeplay (version 2025.0)

For more information on how to install Syclomatic Tool & DPC++ CUDA® plugin, visit [Migrate from CUDA* to C++ with SYCL*](https://www.intel.com/content/www/us/en/developer/tools/oneapi/training/migrate-from-cuda-to-cpp-with-sycl.html#gs.v354cy) <br> How to run SYCL™ applications on NVIDIA® GPUs, refer to oneAPI for NVIDIA GPUs plugin from Codeplay [Install oneAPI for NVIDIA GPUs](https://developer.codeplay.com/products/oneapi/nvidia/)

## CUDA source code evaluation

 This sample test shows an access speed gap between aligned and misaligned structures (those having/missing __align__ keyword). It measures per-element copy throughput for aligned and misaligned structures on big chunks of data. There are two functions in the code named testKernel, a simple CUDA kernel in which copy is carried out on a per-element basis, so it's not per-byte in case of padded structures and the results are validated on the host-side in testCPU function.

This sample is migrated from the NVIDIA CUDA sample. See the sample [alignedTypes](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/6_Performance/alignedTypes) in the NVIDIA/cuda-samples GitHub.
>  **Note**: Refer to [Workflow for a CUDA* to SYCL* Migration](https://www.intel.com/content/www/us/en/developer/tools/oneapi/training/cuda-sycl-migration-workflow.html#gs.s2njvh) for general information about the migration workflow.

## Set Environment Variables

When working with the command-line interface (CLI), you should configure the oneAPI toolkits using environment variables. Set up your CLI environment by sourcing the `setvars` script every time you open a new terminal window. This practice ensures that your compiler, libraries, and tools are ready for development.

## Migrate the `Aligned Types` Sample

### Migrate the Code using SYCLomatic

For this sample, the SYCLomatic tool automatically migrates 100% of the CUDA runtime API's to SYCL. Follow these steps to generate the SYCL code using the compatibility tool:

1. git clone https://github.com/NVIDIA/cuda-samples.git
2. cd cuda-samples/Samples/6_Performance/alignedTypes/
3. Generate a compilation database with intercept-build
   ```
   intercept-build make
   ```
4. The above step creates a JSON file named compile_commands.json with all the compiler invocations and stores the names of the input files and the compiler options.
5. Pass the JSON file as input to the SYCLomatic Tool. The result is written to a folder named dpct_output. The --in-root specifies path to the root of the source tree to be migrated.
   ```
   c2s -p compile_commands.json --in-root ../../.. --gen-helper-function
   ```
### Manual Workaround
CUDA code includes a custom API findCUDADevice in helper_cuda file to find the best CUDA Device available
```
 findCudaDevice (argc, (const char **) argv);   
```
Since its a custom API SYCLomatic tool will not act on it and we can either remove it or replace it with the sycl get_device() API.

## Build the `alignedTypes` Sample for CPU and GPU

> **Note**: If you have not already done so, set up your CLI
> environment by sourcing  the `setvars` script in the root of your oneAPI installation.
>
> Linux*:
> - For system wide installations: `. /opt/intel/oneapi/setvars.sh`
> - For private installations: ` . ~/intel/oneapi/setvars.sh`
> - For non-POSIX shells, like csh, use the following command: `bash -c 'source <install-dir>/setvars.sh ; exec csh'`
>
> For more information on configuring environment variables, see [Use the setvars Script with Linux* or macOS*](https://www.intel.com/content/www/us/en/develop/documentation/oneapi-programming-guide/top/oneapi-development-environment-setup/use-the-setvars-script-with-linux-or-macos.html).

### On Linux*

1. Change to the sample directory.
2. Build the program.
   ```
   $ mkdir build
   $ cd build
   $ cmake .. or ( cmake -D INTEL_MAX_GPU=1 .. ) or ( cmake -D NVIDIA_GPU=1 .. )
   $ make
   ```
>**Note:** 
> - By default, no flags are enabled during the build which supports Intel® UHD Graphics, Intel® Gen9, Gen11, Xeon CPU.
> - Enable INTEL_MAX_GPU flag during build which supports Intel® Data Center GPU Max 1550 or 1100 to get optimized performance.
> - Enable NVIDIA_GPU flag during build which supports NVIDIA GPUs.([oneAPI for NVIDIA GPUs plugin from Codeplay](https://developer.codeplay.com/products/oneapi/nvidia/)  is required to build for NVIDIA GPUs)
   
   By default, this command sequence will build the `sycl_migrated` versions of the program.

3. Run the code

   You can run the programs for CPU and GPU. The commands indicate the device target.

   Run `sycl_migrated` on GPU.
      ```
      make run
      ```
   Run `sycl_migrated` on CPU.
      ```
      export ONEAPI_DEVICE_SELECTOR=opencl:cpu
      make run
      unset ONEAPI_DEVICE_SELECTOR
      ```
#### Troubleshooting

If an error occurs, you can get more details by running `make` with
the `VERBOSE=1` argument:
```
make VERBOSE=1
```
If you receive an error message, troubleshoot the problem using the **Diagnostics Utility for Intel® oneAPI Toolkits**. The diagnostic utility provides configuration and system checks to help find missing dependencies, permissions errors, and other issues. See the [Diagnostics Utility for Intel® oneAPI Toolkits User Guide](https://www.intel.com/content/www/us/en/docs/oneapi/user-guide-diagnostic-utility//2025-0/overview.html) for more information on using the utility.

## License
Code samples are licensed under the MIT license. See
[License.txt](https://github.com/oneapi-src/oneAPI-samples/blob/master/License.txt) for details.

Third-party program licenses are at [third-party-programs.txt](https://github.com/oneapi-src/oneAPI-samples/blob/master/third-party-programs.txt).

