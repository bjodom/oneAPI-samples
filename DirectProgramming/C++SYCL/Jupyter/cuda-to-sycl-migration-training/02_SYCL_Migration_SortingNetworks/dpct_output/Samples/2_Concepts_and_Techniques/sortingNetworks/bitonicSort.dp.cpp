/* Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//Based on http://www.iti.fh-flensburg.de/lang/algorithmen/sortieren/bitonic/bitonicen.htm

#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <assert.h>

#include <helper_cuda.h>
#include "sortingNetworks_common.h"
#include "sortingNetworks_common.dp.hpp"

////////////////////////////////////////////////////////////////////////////////
// Monolithic bitonic sort kernel for short arrays fitting into shared memory
////////////////////////////////////////////////////////////////////////////////
void bitonicSortShared(uint *d_DstKey, uint *d_DstVal,
                                  uint *d_SrcKey, uint *d_SrcVal,
                                  uint arrayLength, uint dir,
                                  const sycl::nd_item<3> &item_ct1, uint *s_key,
                                  uint *s_val) {
  // Handle to thread block group
  sycl::group<3> cta = item_ct1.get_group();
  // Shared memory storage for one or more short vectors

  // Offset to the beginning of subbatch and load data
  d_SrcKey +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_SrcVal +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_DstKey +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_DstVal +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  s_key[item_ct1.get_local_id(2) + 0] = d_SrcKey[0];
  s_val[item_ct1.get_local_id(2) + 0] = d_SrcVal[0];
  s_key[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)] =
      d_SrcKey[(SHARED_SIZE_LIMIT / 2)];
  s_val[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)] =
      d_SrcVal[(SHARED_SIZE_LIMIT / 2)];

  for (uint size = 2; size < arrayLength; size <<= 1) {
    // Bitonic merge
    uint ddd = dir ^ ((item_ct1.get_local_id(2) & (size / 2)) != 0);

    for (uint stride = size / 2; stride > 0; stride >>= 1) {
      /*
      DPCT1065:1: Consider replacing sycl::nd_item::barrier() with
      sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
      performance if there is no access to global memory.
      */
      item_ct1.barrier();
      uint pos = 2 * item_ct1.get_local_id(2) -
                 (item_ct1.get_local_id(2) & (stride - 1));
      Comparator(s_key[pos + 0], s_val[pos + 0], s_key[pos + stride],
                 s_val[pos + stride], ddd);
    }
  }

  // ddd == dir for the last bitonic merge step
  {
    for (uint stride = arrayLength / 2; stride > 0; stride >>= 1) {
      /*
      DPCT1065:2: Consider replacing sycl::nd_item::barrier() with
      sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
      performance if there is no access to global memory.
      */
      item_ct1.barrier();
      uint pos = 2 * item_ct1.get_local_id(2) -
                 (item_ct1.get_local_id(2) & (stride - 1));
      Comparator(s_key[pos + 0], s_val[pos + 0], s_key[pos + stride],
                 s_val[pos + stride], dir);
    }
  }

  /*
  DPCT1065:0: Consider replacing sycl::nd_item::barrier() with
  sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
  performance if there is no access to global memory.
  */
  item_ct1.barrier();
  d_DstKey[0] = s_key[item_ct1.get_local_id(2) + 0];
  d_DstVal[0] = s_val[item_ct1.get_local_id(2) + 0];
  d_DstKey[(SHARED_SIZE_LIMIT / 2)] =
      s_key[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)];
  d_DstVal[(SHARED_SIZE_LIMIT / 2)] =
      s_val[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)];
}

////////////////////////////////////////////////////////////////////////////////
// Bitonic sort kernel for large arrays (not fitting into shared memory)
////////////////////////////////////////////////////////////////////////////////
// Bottom-level bitonic sort
// Almost the same as bitonicSortShared with the exception of
// even / odd subarrays being sorted in opposite directions
// Bitonic merge accepts both
// Ascending | descending or descending | ascending sorted pairs
void bitonicSortShared1(uint *d_DstKey, uint *d_DstVal,
                                   uint *d_SrcKey, uint *d_SrcVal,
                                   const sycl::nd_item<3> &item_ct1, uint *s_key,
                                   uint *s_val) {
  // Handle to thread block group
  sycl::group<3> cta = item_ct1.get_group();
  // Shared memory storage for current subarray

  // Offset to the beginning of subarray and load data
  d_SrcKey +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_SrcVal +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_DstKey +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_DstVal +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  s_key[item_ct1.get_local_id(2) + 0] = d_SrcKey[0];
  s_val[item_ct1.get_local_id(2) + 0] = d_SrcVal[0];
  s_key[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)] =
      d_SrcKey[(SHARED_SIZE_LIMIT / 2)];
  s_val[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)] =
      d_SrcVal[(SHARED_SIZE_LIMIT / 2)];

  for (uint size = 2; size < SHARED_SIZE_LIMIT; size <<= 1) {
    // Bitonic merge
    uint ddd = (item_ct1.get_local_id(2) & (size / 2)) != 0;

    for (uint stride = size / 2; stride > 0; stride >>= 1) {
      /*
      DPCT1065:4: Consider replacing sycl::nd_item::barrier() with
      sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
      performance if there is no access to global memory.
      */
      item_ct1.barrier();
      uint pos = 2 * item_ct1.get_local_id(2) -
                 (item_ct1.get_local_id(2) & (stride - 1));
      Comparator(s_key[pos + 0], s_val[pos + 0], s_key[pos + stride],
                 s_val[pos + stride], ddd);
    }
  }

  // Odd / even arrays of SHARED_SIZE_LIMIT elements
  // sorted in opposite directions
  uint ddd = item_ct1.get_group(2) & 1;
  {
    for (uint stride = SHARED_SIZE_LIMIT / 2; stride > 0; stride >>= 1) {
      /*
      DPCT1065:5: Consider replacing sycl::nd_item::barrier() with
      sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
      performance if there is no access to global memory.
      */
      item_ct1.barrier();
      uint pos = 2 * item_ct1.get_local_id(2) -
                 (item_ct1.get_local_id(2) & (stride - 1));
      Comparator(s_key[pos + 0], s_val[pos + 0], s_key[pos + stride],
                 s_val[pos + stride], ddd);
    }
  }

  /*
  DPCT1065:3: Consider replacing sycl::nd_item::barrier() with
  sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
  performance if there is no access to global memory.
  */
  item_ct1.barrier();
  d_DstKey[0] = s_key[item_ct1.get_local_id(2) + 0];
  d_DstVal[0] = s_val[item_ct1.get_local_id(2) + 0];
  d_DstKey[(SHARED_SIZE_LIMIT / 2)] =
      s_key[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)];
  d_DstVal[(SHARED_SIZE_LIMIT / 2)] =
      s_val[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)];
}

// Bitonic merge iteration for stride >= SHARED_SIZE_LIMIT
void bitonicMergeGlobal(uint *d_DstKey, uint *d_DstVal,
                                   uint *d_SrcKey, uint *d_SrcVal,
                                   uint arrayLength, uint size, uint stride,
                                   uint dir, const sycl::nd_item<3> &item_ct1) {
  uint global_comparatorI =
      item_ct1.get_group(2) * item_ct1.get_local_range(2) +
      item_ct1.get_local_id(2);
  uint comparatorI = global_comparatorI & (arrayLength / 2 - 1);

  // Bitonic merge
  uint ddd = dir ^ ((comparatorI & (size / 2)) != 0);
  uint pos = 2 * global_comparatorI - (global_comparatorI & (stride - 1));

  uint keyA = d_SrcKey[pos + 0];
  uint valA = d_SrcVal[pos + 0];
  uint keyB = d_SrcKey[pos + stride];
  uint valB = d_SrcVal[pos + stride];

  Comparator(keyA, valA, keyB, valB, ddd);

  d_DstKey[pos + 0] = keyA;
  d_DstVal[pos + 0] = valA;
  d_DstKey[pos + stride] = keyB;
  d_DstVal[pos + stride] = valB;
}

// Combined bitonic merge steps for
// size > SHARED_SIZE_LIMIT and stride = [1 .. SHARED_SIZE_LIMIT / 2]
void bitonicMergeShared(uint *d_DstKey, uint *d_DstVal,
                                   uint *d_SrcKey, uint *d_SrcVal,
                                   uint arrayLength, uint size, uint dir,
                                   const sycl::nd_item<3> &item_ct1, uint *s_key,
                                   uint *s_val) {
  // Handle to thread block group
  sycl::group<3> cta = item_ct1.get_group();
  // Shared memory storage for current subarray

  d_SrcKey +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_SrcVal +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_DstKey +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  d_DstVal +=
      item_ct1.get_group(2) * SHARED_SIZE_LIMIT + item_ct1.get_local_id(2);
  s_key[item_ct1.get_local_id(2) + 0] = d_SrcKey[0];
  s_val[item_ct1.get_local_id(2) + 0] = d_SrcVal[0];
  s_key[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)] =
      d_SrcKey[(SHARED_SIZE_LIMIT / 2)];
  s_val[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)] =
      d_SrcVal[(SHARED_SIZE_LIMIT / 2)];

  // Bitonic merge
  uint comparatorI = UMAD(item_ct1.get_group(2), item_ct1.get_local_range(2),
                          item_ct1.get_local_id(2)) &
                     ((arrayLength / 2) - 1);
  uint ddd = dir ^ ((comparatorI & (size / 2)) != 0);

  for (uint stride = SHARED_SIZE_LIMIT / 2; stride > 0; stride >>= 1) {
    /*
    DPCT1065:7: Consider replacing sycl::nd_item::barrier() with
    sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
    performance if there is no access to global memory.
    */
    item_ct1.barrier();
    uint pos = 2 * item_ct1.get_local_id(2) -
               (item_ct1.get_local_id(2) & (stride - 1));
    Comparator(s_key[pos + 0], s_val[pos + 0], s_key[pos + stride],
               s_val[pos + stride], ddd);
  }

  /*
  DPCT1065:6: Consider replacing sycl::nd_item::barrier() with
  sycl::nd_item::barrier(sycl::access::fence_space::local_space) for better
  performance if there is no access to global memory.
  */
  item_ct1.barrier();
  d_DstKey[0] = s_key[item_ct1.get_local_id(2) + 0];
  d_DstVal[0] = s_val[item_ct1.get_local_id(2) + 0];
  d_DstKey[(SHARED_SIZE_LIMIT / 2)] =
      s_key[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)];
  d_DstVal[(SHARED_SIZE_LIMIT / 2)] =
      s_val[item_ct1.get_local_id(2) + (SHARED_SIZE_LIMIT / 2)];
}

////////////////////////////////////////////////////////////////////////////////
// Interface function
////////////////////////////////////////////////////////////////////////////////
// Helper function (also used by odd-even merge sort)
extern "C" uint factorRadix2(uint *log2L, uint L) {
  if (!L) {
    *log2L = 0;
    return 0;
  } else {
    for (*log2L = 0; (L & 1) == 0; L >>= 1, *log2L++)
      ;

    return L;
  }
}

extern "C" uint bitonicSort(uint *d_DstKey, uint *d_DstVal, uint *d_SrcKey,
                            uint *d_SrcVal, uint batchSize, uint arrayLength,
                            uint dir) {
  // Nothing to sort
  if (arrayLength < 2) return 0;

  // Only power-of-two array lengths are supported by this implementation
  uint log2L;
  uint factorizationRemainder = factorRadix2(&log2L, arrayLength);
  assert(factorizationRemainder == 1);

  dir = (dir != 0);

  uint blockCount = batchSize * arrayLength / SHARED_SIZE_LIMIT;
  uint threadCount = SHARED_SIZE_LIMIT / 2;

  if (arrayLength <= SHARED_SIZE_LIMIT) {
    assert((batchSize * arrayLength) % SHARED_SIZE_LIMIT == 0);
    /*
    DPCT1049:8: The work-group size passed to the SYCL kernel may exceed the
    limit. To get the device limit, query info::device::max_work_group_size.
    Adjust the work-group size if needed.
    */
    dpct::get_in_order_queue().submit([&](sycl::handler &cgh) {
      /*
      DPCT1101:30: 'SHARED_SIZE_LIMIT' expression was replaced with a value.
      Modify the code to use the original expression, provided in comments, if
      it is correct.
      */
      sycl::local_accessor<uint, 1> s_key_acc_ct1(
          sycl::range<1>(1024 /*SHARED_SIZE_LIMIT*/), cgh);
      /*
      DPCT1101:31: 'SHARED_SIZE_LIMIT' expression was replaced with a value.
      Modify the code to use the original expression, provided in comments, if
      it is correct.
      */
      sycl::local_accessor<uint, 1> s_val_acc_ct1(
          sycl::range<1>(1024 /*SHARED_SIZE_LIMIT*/), cgh);

      cgh.parallel_for(
          sycl::nd_range<3>(sycl::range<3>(1, 1, blockCount) *
                                sycl::range<3>(1, 1, threadCount),
                            sycl::range<3>(1, 1, threadCount)),
          [=](sycl::nd_item<3> item_ct1) {
            bitonicSortShared(
                d_DstKey, d_DstVal, d_SrcKey, d_SrcVal, arrayLength, dir,
                item_ct1,
                s_key_acc_ct1.get_multi_ptr<sycl::access::decorated::no>()
                    .get(),
                s_val_acc_ct1.get_multi_ptr<sycl::access::decorated::no>()
                    .get());
          });
    });
  } else {
    /*
    DPCT1049:9: The work-group size passed to the SYCL kernel may exceed the
    limit. To get the device limit, query info::device::max_work_group_size.
    Adjust the work-group size if needed.
    */
    dpct::get_in_order_queue().submit([&](sycl::handler &cgh) {
      /*
      DPCT1101:32: 'SHARED_SIZE_LIMIT' expression was replaced with a value.
      Modify the code to use the original expression, provided in comments, if
      it is correct.
      */
      sycl::local_accessor<uint, 1> s_key_acc_ct1(
          sycl::range<1>(1024 /*SHARED_SIZE_LIMIT*/), cgh);
      /*
      DPCT1101:33: 'SHARED_SIZE_LIMIT' expression was replaced with a value.
      Modify the code to use the original expression, provided in comments, if
      it is correct.
      */
      sycl::local_accessor<uint, 1> s_val_acc_ct1(
          sycl::range<1>(1024 /*SHARED_SIZE_LIMIT*/), cgh);

      cgh.parallel_for(
          sycl::nd_range<3>(sycl::range<3>(1, 1, blockCount) *
                                sycl::range<3>(1, 1, threadCount),
                            sycl::range<3>(1, 1, threadCount)),
          [=](sycl::nd_item<3> item_ct1) {
            bitonicSortShared1(
                d_DstKey, d_DstVal, d_SrcKey, d_SrcVal, item_ct1,
                s_key_acc_ct1.get_multi_ptr<sycl::access::decorated::no>()
                    .get(),
                s_val_acc_ct1.get_multi_ptr<sycl::access::decorated::no>()
                    .get());
          });
    });

    for (uint size = 2 * SHARED_SIZE_LIMIT; size <= arrayLength; size <<= 1)
      for (unsigned stride = size / 2; stride > 0; stride >>= 1)
        if (stride >= SHARED_SIZE_LIMIT) {
          dpct::get_in_order_queue().parallel_for(
              sycl::nd_range<3>(
                  sycl::range<3>(1, 1, (batchSize * arrayLength) / 512) *
                      sycl::range<3>(1, 1, 256),
                  sycl::range<3>(1, 1, 256)),
              [=](sycl::nd_item<3> item_ct1) {
                bitonicMergeGlobal(d_DstKey, d_DstVal, d_DstKey, d_DstVal,
                                   arrayLength, size, stride, dir, item_ct1);
              });
        } else {
          /*
          DPCT1049:10: The work-group size passed to the SYCL kernel may exceed
          the limit. To get the device limit, query
          info::device::max_work_group_size. Adjust the work-group size if
          needed.
          */
          dpct::get_in_order_queue().submit([&](sycl::handler &cgh) {
            /*
            DPCT1101:34: 'SHARED_SIZE_LIMIT' expression was replaced with a
            value. Modify the code to use the original expression, provided in
            comments, if it is correct.
            */
            sycl::local_accessor<uint, 1> s_key_acc_ct1(
                sycl::range<1>(1024 /*SHARED_SIZE_LIMIT*/), cgh);
            /*
            DPCT1101:35: 'SHARED_SIZE_LIMIT' expression was replaced with a
            value. Modify the code to use the original expression, provided in
            comments, if it is correct.
            */
            sycl::local_accessor<uint, 1> s_val_acc_ct1(
                sycl::range<1>(1024 /*SHARED_SIZE_LIMIT*/), cgh);

            cgh.parallel_for(
                sycl::nd_range<3>(sycl::range<3>(1, 1, blockCount) *
                                      sycl::range<3>(1, 1, threadCount),
                                  sycl::range<3>(1, 1, threadCount)),
                [=](sycl::nd_item<3> item_ct1) {
                  bitonicMergeShared(
                      d_DstKey, d_DstVal, d_DstKey, d_DstVal, arrayLength, size,
                      dir, item_ct1,
                      s_key_acc_ct1.get_multi_ptr<sycl::access::decorated::no>()
                          .get(),
                      s_val_acc_ct1.get_multi_ptr<sycl::access::decorated::no>()
                          .get());
                });
          });
          break;
        }
  }

  return threadCount;
}
