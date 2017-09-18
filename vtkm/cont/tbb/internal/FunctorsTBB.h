//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2014 Sandia Corporation.
//  Copyright 2014 UT-Battelle, LLC.
//  Copyright 2014 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#ifndef vtk_m_cont_tbb_internal_FunctorsTBB_h
#define vtk_m_cont_tbb_internal_FunctorsTBB_h

#include <vtkm/TypeTraits.h>
#include <vtkm/Types.h>
#include <vtkm/cont/ArrayPortalToIterators.h>
#include <vtkm/cont/Error.h>
#include <vtkm/cont/internal/FunctorsGeneral.h>
#include <vtkm/exec/internal/ErrorMessageBuffer.h>

#include <algorithm>
#include <sstream>

VTKM_THIRDPARTY_PRE_INCLUDE

#if defined(VTKM_MSVC)

// TBB's header include a #pragma comment(lib,"tbb.lib") line to make all
// consuming libraries link to tbb, this is bad behavior in a header
// based project
#pragma push_macro("__TBB_NO_IMPLICITLINKAGE")
#define __TBB_NO_IMPLICIT_LINKAGE 1

#endif // defined(VTKM_MSVC)

// TBB includes windows.h, so instead we want to include windows.h with the
// correct settings so that we don't clobber any existing function
#include <vtkm/internal/Windows.h>

#include <tbb/tbb_stddef.h>
#if (TBB_VERSION_MAJOR == 4) && (TBB_VERSION_MINOR == 2)
//we provide an patched implementation of tbb parallel_sort
//that fixes ADL for std::swap. This patch has been submitted to Intel
//and is fixed in TBB 4.2 update 2.
#include <vtkm/cont/tbb/internal/parallel_sort.h>
#else
#include <tbb/parallel_sort.h>
#endif

#include <numeric>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range3d.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_scan.h>
#include <tbb/partitioner.h>
#include <tbb/tick_count.h>

#if defined(VTKM_MSVC)
#pragma pop_macro("__TBB_NO_IMPLICITLINKAGE")
#endif

VTKM_THIRDPARTY_POST_INCLUDE

namespace vtkm
{
namespace cont
{
namespace tbb
{

namespace internal
{
template <typename ResultType, typename Function>
using WrappedBinaryOperator = vtkm::cont::internal::WrappedBinaryOperator<ResultType, Function>;
}

// The "grain size" of scheduling with TBB.  Not a lot of thought has gone
// into picking this size.
static const vtkm::Id TBB_GRAIN_SIZE = 1024;

template <class InputPortalType, class T, class BinaryOperationType>
struct ReduceBody
{
  T Sum;
  T InitialValue;
  bool FirstCall;
  InputPortalType InputPortal;
  BinaryOperationType BinaryOperation;

  VTKM_CONT
  ReduceBody(const InputPortalType& inputPortal,
             T initialValue,
             BinaryOperationType binaryOperation)
    : Sum(vtkm::TypeTraits<T>::ZeroInitialization())
    , InitialValue(initialValue)
    , FirstCall(true)
    , InputPortal(inputPortal)
    , BinaryOperation(binaryOperation)
  {
  }

  VTKM_EXEC_CONT
  ReduceBody(const ReduceBody& body, ::tbb::split)
    : Sum(vtkm::TypeTraits<T>::ZeroInitialization())
    , InitialValue(body.InitialValue)
    , FirstCall(true)
    , InputPortal(body.InputPortal)
    , BinaryOperation(body.BinaryOperation)
  {
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range)
  {
    using InputIteratorsType = vtkm::cont::ArrayPortalToIterators<InputPortalType>;
    InputIteratorsType inputIterators(this->InputPortal);

    //use temp, and iterators instead of member variable to reduce false sharing
    typename InputIteratorsType::IteratorType inIter =
      inputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());

    T temp = this->BinaryOperation(*inIter, *(inIter + 1));
    ++inIter;
    ++inIter;
    for (vtkm::Id index = range.begin() + 2; index != range.end(); ++index, ++inIter)
    {
      temp = this->BinaryOperation(temp, *inIter);
    }

    //determine if we also have to add the initial value to temp
    if (range.begin() == 0)
    {
      temp = this->BinaryOperation(temp, this->InitialValue);
    }

    //Now we can save temp back to sum, taking into account if
    //this task has been called before, and the sum value needs
    //to also be reduced.
    if (this->FirstCall)
    {
      this->Sum = temp;
    }
    else
    {
      this->Sum = this->BinaryOperation(this->Sum, temp);
    }

    this->FirstCall = false;
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC_CONT
  void join(const ReduceBody& left)
  {
    // std::cout << "join" << std::endl;
    this->Sum = this->BinaryOperation(left.Sum, this->Sum);
  }
};

VTKM_SUPPRESS_EXEC_WARNINGS
template <class InputPortalType, typename T, class BinaryOperationType>
VTKM_CONT static T ReducePortals(InputPortalType inputPortal,
                                 T initialValue,
                                 BinaryOperationType binaryOperation)
{
  using WrappedBinaryOp = internal::WrappedBinaryOperator<T, BinaryOperationType>;

  WrappedBinaryOp wrappedBinaryOp(binaryOperation);
  ReduceBody<InputPortalType, T, WrappedBinaryOp> body(inputPortal, initialValue, wrappedBinaryOp);
  vtkm::Id arrayLength = inputPortal.GetNumberOfValues();

  if (arrayLength > 1)
  {
    ::tbb::blocked_range<vtkm::Id> range(0, arrayLength, TBB_GRAIN_SIZE);
    ::tbb::parallel_reduce(range, body);
    return body.Sum;
  }
  else if (arrayLength == 1)
  {
    //ReduceBody does not work with an array of size 1.
    return binaryOperation(initialValue, inputPortal.Get(0));
  }
  else // arrayLength == 0
  {
    // ReduceBody does not work with an array of size 0.
    return initialValue;
  }
}

// Define this to print out timing information from the reduction and join
// operations in the tbb ReduceByKey algorithm:
//#define _VTKM_DEBUG_TBB_RBK

template <typename KeysInPortalType,
          typename ValuesInPortalType,
          typename KeysOutPortalType,
          typename ValuesOutPortalType,
          class BinaryOperationType>
struct ReduceByKeyBody
{
  using KeyType = typename KeysInPortalType::ValueType;
  using ValueType = typename ValuesInPortalType::ValueType;

  struct Range
  {
    vtkm::Id InputBegin;
    vtkm::Id InputEnd;
    vtkm::Id OutputBegin;
    vtkm::Id OutputEnd;

    VTKM_EXEC_CONT
    Range()
      : InputBegin(-1)
      , InputEnd(-1)
      , OutputBegin(-1)
      , OutputEnd(-1)
    {
    }

    VTKM_EXEC_CONT
    Range(vtkm::Id inputBegin, vtkm::Id inputEnd, vtkm::Id outputBegin, vtkm::Id outputEnd)
      : InputBegin(inputBegin)
      , InputEnd(inputEnd)
      , OutputBegin(outputBegin)
      , OutputEnd(outputEnd)
    {
      this->AssertSane();
    }

    VTKM_EXEC_CONT
    void AssertSane() const
    {
      VTKM_ASSERT("Input begin precedes end" && this->InputBegin <= this->InputEnd);
      VTKM_ASSERT("Output begin precedes end" && this->OutputBegin <= this->OutputEnd);
      VTKM_ASSERT("Output not past input" && this->OutputBegin <= this->InputBegin &&
                  this->OutputEnd <= this->InputEnd);
      VTKM_ASSERT("Output smaller than input" &&
                  (this->OutputEnd - this->OutputBegin) <= (this->InputEnd - this->InputBegin));
    }

    VTKM_EXEC_CONT
    bool IsNext(const Range& next) const { return this->InputEnd == next.InputBegin; }
  };

  KeysInPortalType KeysInPortal;
  ValuesInPortalType ValuesInPortal;
  KeysOutPortalType KeysOutPortal;
  ValuesOutPortalType ValuesOutPortal;
  BinaryOperationType BinaryOperation;
  Range Ranges;
#ifdef _VTKM_DEBUG_TBB_RBK
  double ReduceTime;
  double JoinTime;
#endif

  VTKM_CONT
  ReduceByKeyBody(const KeysInPortalType& keysInPortal,
                  const ValuesInPortalType& valuesInPortal,
                  const KeysOutPortalType& keysOutPortal,
                  const ValuesOutPortalType& valuesOutPortal,
                  BinaryOperationType binaryOperation)
    : KeysInPortal(keysInPortal)
    , ValuesInPortal(valuesInPortal)
    , KeysOutPortal(keysOutPortal)
    , ValuesOutPortal(valuesOutPortal)
    , BinaryOperation(binaryOperation)
#ifdef _VTKM_DEBUG_TBB_RBK
    , ReduceTime(0)
    , JoinTime(0)
#endif
  {
  }

  VTKM_EXEC_CONT
  ReduceByKeyBody(const ReduceByKeyBody& body, ::tbb::split)
    : KeysInPortal(body.KeysInPortal)
    , ValuesInPortal(body.ValuesInPortal)
    , KeysOutPortal(body.KeysOutPortal)
    , ValuesOutPortal(body.ValuesOutPortal)
    , BinaryOperation(body.BinaryOperation)
#ifdef _VTKM_DEBUG_TBB_RBK
    , ReduceTime(0)
    , JoinTime(0)
#endif
  {
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range)
  {
#ifdef _VTKM_DEBUG_TBB_RBK
    ::tbb::tick_count startTime = ::tbb::tick_count::now();
#endif // _VTKM_DEBUG_TBB_RBK
    if (range.empty())
    {
      return;
    }

    bool firstRun = this->Ranges.OutputBegin < 0; // First use of this body object
    if (firstRun)
    {
      this->Ranges.InputBegin = range.begin();
    }
    else
    {
      // Must be a continuation of the previous input range:
      VTKM_ASSERT(this->Ranges.InputEnd == range.begin());
    }
    this->Ranges.InputEnd = range.end();
    this->Ranges.AssertSane();

    using KeysInIteratorsType = vtkm::cont::ArrayPortalToIterators<KeysInPortalType>;
    using ValuesInIteratorsType = vtkm::cont::ArrayPortalToIterators<ValuesInPortalType>;
    using KeysOutIteratorsType = vtkm::cont::ArrayPortalToIterators<KeysOutPortalType>;
    using ValuesOutIteratorsType = vtkm::cont::ArrayPortalToIterators<ValuesOutPortalType>;

    KeysInIteratorsType keysInIters(this->KeysInPortal);
    ValuesInIteratorsType valuesInIters(this->ValuesInPortal);
    KeysOutIteratorsType keysOutIters(this->KeysOutPortal);
    ValuesOutIteratorsType valuesOutIters(this->ValuesOutPortal);

    using KeysInIteratorType = typename KeysInIteratorsType::IteratorType;
    using ValuesInIteratorType = typename ValuesInIteratorsType::IteratorType;
    using KeysOutIteratorType = typename KeysOutIteratorsType::IteratorType;
    using ValuesOutIteratorType = typename ValuesOutIteratorsType::IteratorType;

    KeysInIteratorType keysIn = keysInIters.GetBegin();
    ValuesInIteratorType valuesIn = valuesInIters.GetBegin();
    KeysOutIteratorType keysOut = keysOutIters.GetBegin();
    ValuesOutIteratorType valuesOut = valuesOutIters.GetBegin();

    vtkm::Id readPos = range.begin();
    const vtkm::Id readEnd = range.end();

    // Determine output index. If we're reusing the body, pick up where the
    // last block left off. If not, use the input range.
    vtkm::Id writePos;
    if (firstRun)
    {
      this->Ranges.OutputBegin = range.begin();
      this->Ranges.OutputEnd = range.begin();
      writePos = range.begin();
    }
    else
    {
      writePos = this->Ranges.OutputEnd;
    }
    this->Ranges.AssertSane();

    // We're either writing at the end of a previous block, or at the input
    // location. Either way, the write position will never be greater than
    // the read position.
    VTKM_ASSERT(writePos <= readPos);

    // Initialize reduction variables:
    BinaryOperationType functor(this->BinaryOperation);
    KeyType currentKey = keysIn[readPos];
    ValueType currentValue = valuesIn[readPos];
    ++readPos;

    // If the start of the current range continues a previous key block,
    // initialize with the previous result and decrement the write index.
    VTKM_ASSERT(firstRun || writePos > 0);
    if (!firstRun && keysOut[writePos - 1] == currentKey)
    {
      // Ensure that we'll overwrite the continued key values:
      --writePos;

      // Update our accumulator with the partial value:
      currentValue = functor(valuesOut[writePos], currentValue);
    }

    // Special case: single value in range
    if (readPos >= readEnd)
    {
      keysOut[writePos] = currentKey;
      valuesOut[writePos] = currentValue;
      ++writePos;

      this->Ranges.OutputEnd = writePos;
      return;
    }

    for (;;)
    {
      while (readPos < readEnd && currentKey == keysIn[readPos])
      {
        currentValue = functor(currentValue, valuesIn[readPos]);
        ++readPos;
      }

      VTKM_ASSERT(writePos <= readPos);
      keysOut[writePos] = currentKey;
      valuesOut[writePos] = currentValue;
      ++writePos;

      if (readPos < readEnd)
      {
        currentKey = keysIn[readPos];
        currentValue = valuesIn[readPos];
        ++readPos;
        continue;
      }

      break;
    }

    this->Ranges.OutputEnd = writePos;

#ifdef _VTKM_DEBUG_TBB_RBK
    ::tbb::tick_count endTime = ::tbb::tick_count::now();
    double time = (endTime - startTime).seconds();
    this->ReduceTime += time;
    std::ostringstream out;
    out << "Reduced " << range.size() << " key/value pairs in " << time << "s. "
        << "InRange: " << this->Ranges.InputBegin << " " << this->Ranges.InputEnd << " "
        << "OutRange: " << this->Ranges.OutputBegin << " " << this->Ranges.OutputEnd << "\n";
    std::cerr << out.str();
#endif
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC_CONT
  void join(const ReduceByKeyBody& rhs)
  {
    using KeysIteratorsType = vtkm::cont::ArrayPortalToIterators<KeysOutPortalType>;
    using ValuesIteratorsType = vtkm::cont::ArrayPortalToIterators<ValuesOutPortalType>;
    using KeysIteratorType = typename KeysIteratorsType::IteratorType;
    using ValuesIteratorType = typename ValuesIteratorsType::IteratorType;

#ifdef _VTKM_DEBUG_TBB_RBK
    ::tbb::tick_count startTime = ::tbb::tick_count::now();
#endif

    this->Ranges.AssertSane();
    rhs.Ranges.AssertSane();
    // Ensure that we're joining two consecutive subsets of the input:
    VTKM_ASSERT(this->Ranges.IsNext(rhs.Ranges));

    KeysIteratorsType keysIters(this->KeysOutPortal);
    ValuesIteratorsType valuesIters(this->ValuesOutPortal);
    KeysIteratorType keys = keysIters.GetBegin();
    ValuesIteratorType values = valuesIters.GetBegin();

    const vtkm::Id dstBegin = this->Ranges.OutputEnd;
    const vtkm::Id lastDstIdx = this->Ranges.OutputEnd - 1;

    vtkm::Id srcBegin = rhs.Ranges.OutputBegin;
    const vtkm::Id srcEnd = rhs.Ranges.OutputEnd;

    // Merge boundaries if needed:
    if (keys[srcBegin] == keys[lastDstIdx])
    {
      values[lastDstIdx] = this->BinaryOperation(values[lastDstIdx], values[srcBegin]);
      ++srcBegin; // Don't copy the key/value we just reduced
    }

    // move data:
    if (srcBegin != dstBegin && srcBegin != srcEnd)
    {
      // Sanity check:
      VTKM_ASSERT(srcBegin < srcEnd);
      // Not necessary for the copy call to be safe, but if the src range
      // overlaps with the dst range there's a problem with the algorithm:
      VTKM_ASSERT(dstBegin + (srcEnd - srcBegin) <= srcBegin);
      std::copy(keys + srcBegin, keys + srcEnd, keys + dstBegin);
      std::copy(values + srcBegin, values + srcEnd, values + dstBegin);
    }

    this->Ranges.InputEnd = rhs.Ranges.InputEnd;
    this->Ranges.OutputEnd += srcEnd - srcBegin;
    this->Ranges.AssertSane();

#ifdef _VTKM_DEBUG_TBB_RBK
    ::tbb::tick_count endTime = ::tbb::tick_count::now();
    double time = (endTime - startTime).seconds();
    this->JoinTime += rhs.JoinTime + time;
    std::ostringstream out;
    out << "Joined " << (srcEnd - srcBegin) << " rhs values into body in " << time << "s. "
        << "InRange: " << this->Ranges.InputBegin << " " << this->Ranges.InputEnd << " "
        << "OutRange: " << this->Ranges.OutputBegin << " " << this->Ranges.OutputEnd << "\n";
    std::cerr << out.str();
#endif
  }
};

VTKM_SUPPRESS_EXEC_WARNINGS
template <typename KeysInPortalType,
          typename ValuesInPortalType,
          typename KeysOutPortalType,
          typename ValuesOutPortalType,
          typename BinaryOperationType>
VTKM_CONT vtkm::Id ReduceByKeyPortals(KeysInPortalType keysInPortal,
                                      ValuesInPortalType valuesInPortal,
                                      KeysOutPortalType keysOutPortal,
                                      ValuesOutPortalType valuesOutPortal,
                                      BinaryOperationType binaryOperation)
{
  const vtkm::Id inputLength = keysInPortal.GetNumberOfValues();
  VTKM_ASSERT(inputLength == valuesInPortal.GetNumberOfValues());

  if (inputLength == 0)
  {
    return 0;
  }

  using ValueType = typename ValuesInPortalType::ValueType;
  using WrappedBinaryOp = internal::WrappedBinaryOperator<ValueType, BinaryOperationType>;
  WrappedBinaryOp wrappedBinaryOp(binaryOperation);

  ReduceByKeyBody<KeysInPortalType,
                  ValuesInPortalType,
                  KeysOutPortalType,
                  ValuesOutPortalType,
                  WrappedBinaryOp>
    body(keysInPortal, valuesInPortal, keysOutPortal, valuesOutPortal, wrappedBinaryOp);
  ::tbb::blocked_range<vtkm::Id> range(0, inputLength, TBB_GRAIN_SIZE);

#ifdef _VTKM_DEBUG_TBB_RBK
  std::cerr << "\n\nTBB ReduceByKey:\n";
#endif

  ::tbb::parallel_reduce(range, body);

#ifdef _VTKM_DEBUG_TBB_RBK
  std::cerr << "Total reduce time: " << body.ReduceTime << "s\n";
  std::cerr << "Total join time:   " << body.JoinTime << "s\n";
  std::cerr << "\nend\n";
#endif

  body.Ranges.AssertSane();
  VTKM_ASSERT(body.Ranges.InputBegin == 0 && body.Ranges.InputEnd == inputLength &&
              body.Ranges.OutputBegin == 0 && body.Ranges.OutputEnd <= inputLength);

  return body.Ranges.OutputEnd;
}

#ifdef _VTKM_DEBUG_TBB_RBK
#undef _VTKM_DEBUG_TBB_RBK
#endif

template <class InputPortalType, class OutputPortalType, class BinaryOperationType>
struct ScanInclusiveBody
{
  using ValueType = typename std::remove_reference<typename OutputPortalType::ValueType>::type;
  ValueType Sum;
  bool FirstCall;
  InputPortalType InputPortal;
  OutputPortalType OutputPortal;
  BinaryOperationType BinaryOperation;

  VTKM_CONT
  ScanInclusiveBody(const InputPortalType& inputPortal,
                    const OutputPortalType& outputPortal,
                    BinaryOperationType binaryOperation)
    : Sum(vtkm::TypeTraits<ValueType>::ZeroInitialization())
    , FirstCall(true)
    , InputPortal(inputPortal)
    , OutputPortal(outputPortal)
    , BinaryOperation(binaryOperation)
  {
  }

  VTKM_EXEC_CONT
  ScanInclusiveBody(const ScanInclusiveBody& body, ::tbb::split)
    : Sum(vtkm::TypeTraits<ValueType>::ZeroInitialization())
    , FirstCall(true)
    , InputPortal(body.InputPortal)
    , OutputPortal(body.OutputPortal)
    , BinaryOperation(body.BinaryOperation)
  {
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range, ::tbb::pre_scan_tag)
  {
    using InputIteratorsType = vtkm::cont::ArrayPortalToIterators<InputPortalType>;
    InputIteratorsType inputIterators(this->InputPortal);

    //use temp, and iterators instead of member variable to reduce false sharing
    typename InputIteratorsType::IteratorType inIter =
      inputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());
    ValueType temp = this->FirstCall ? *inIter++ : this->BinaryOperation(this->Sum, *inIter++);
    this->FirstCall = false;
    for (vtkm::Id index = range.begin() + 1; index != range.end(); ++index, ++inIter)
    {
      temp = this->BinaryOperation(temp, *inIter);
    }
    this->Sum = temp;
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range, ::tbb::final_scan_tag)
  {
    using InputIteratorsType = vtkm::cont::ArrayPortalToIterators<InputPortalType>;
    using OutputIteratorsType = vtkm::cont::ArrayPortalToIterators<OutputPortalType>;

    InputIteratorsType inputIterators(this->InputPortal);
    OutputIteratorsType outputIterators(this->OutputPortal);

    //use temp, and iterators instead of member variable to reduce false sharing
    typename InputIteratorsType::IteratorType inIter =
      inputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());
    typename OutputIteratorsType::IteratorType outIter =
      outputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());
    ValueType temp = this->FirstCall ? *inIter++ : this->BinaryOperation(this->Sum, *inIter++);
    this->FirstCall = false;
    *outIter++ = temp;
    for (vtkm::Id index = range.begin() + 1; index != range.end(); ++index, ++inIter, ++outIter)
    {
      *outIter = temp = this->BinaryOperation(temp, *inIter);
    }
    this->Sum = temp;
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC_CONT
  void reverse_join(const ScanInclusiveBody& left)
  {
    this->Sum = this->BinaryOperation(left.Sum, this->Sum);
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC_CONT
  void assign(const ScanInclusiveBody& src) { this->Sum = src.Sum; }
};

template <class InputPortalType, class OutputPortalType, class BinaryOperationType>
struct ScanExclusiveBody
{
  using ValueType = typename std::remove_reference<typename OutputPortalType::ValueType>::type;

  ValueType Sum;
  bool FirstCall;
  InputPortalType InputPortal;
  OutputPortalType OutputPortal;
  BinaryOperationType BinaryOperation;

  VTKM_CONT
  ScanExclusiveBody(const InputPortalType& inputPortal,
                    const OutputPortalType& outputPortal,
                    BinaryOperationType binaryOperation,
                    const ValueType& initialValue)
    : Sum(initialValue)
    , FirstCall(true)
    , InputPortal(inputPortal)
    , OutputPortal(outputPortal)
    , BinaryOperation(binaryOperation)
  {
  }

  VTKM_EXEC_CONT
  ScanExclusiveBody(const ScanExclusiveBody& body, ::tbb::split)
    : Sum(body.Sum)
    , FirstCall(true)
    , InputPortal(body.InputPortal)
    , OutputPortal(body.OutputPortal)
    , BinaryOperation(body.BinaryOperation)
  {
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range, ::tbb::pre_scan_tag)
  {
    using InputIteratorsType = vtkm::cont::ArrayPortalToIterators<InputPortalType>;
    InputIteratorsType inputIterators(this->InputPortal);

    //move the iterator to the first item
    typename InputIteratorsType::IteratorType iter =
      inputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());

    ValueType temp = *iter;
    ++iter;
    if (!(this->FirstCall && range.begin() > 0))
    {
      temp = this->BinaryOperation(this->Sum, temp);
    }
    for (vtkm::Id index = range.begin() + 1; index != range.end(); ++index, ++iter)
    {
      temp = this->BinaryOperation(temp, *iter);
    }
    this->Sum = temp;
    this->FirstCall = false;
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range, ::tbb::final_scan_tag)
  {
    using InputIteratorsType = vtkm::cont::ArrayPortalToIterators<InputPortalType>;
    using OutputIteratorsType = vtkm::cont::ArrayPortalToIterators<OutputPortalType>;

    InputIteratorsType inputIterators(this->InputPortal);
    OutputIteratorsType outputIterators(this->OutputPortal);

    //move the iterators to the first item
    typename InputIteratorsType::IteratorType inIter =
      inputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());
    typename OutputIteratorsType::IteratorType outIter =
      outputIterators.GetBegin() + static_cast<std::ptrdiff_t>(range.begin());

    ValueType temp = this->Sum;
    for (vtkm::Id index = range.begin(); index != range.end(); ++index, ++inIter, ++outIter)
    {
      //copy into a local reference since Input and Output portal
      //could point to the same memory location
      ValueType v = *inIter;
      *outIter = temp;
      temp = this->BinaryOperation(temp, v);
    }
    this->Sum = temp;
    this->FirstCall = false;
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC_CONT
  void reverse_join(const ScanExclusiveBody& left)
  {
    //The contract we have with TBB is that they will only join
    //two objects that have been scanned, or two objects which
    //haven't been scanned
    VTKM_ASSERT(left.FirstCall == this->FirstCall);
    if (!left.FirstCall && !this->FirstCall)
    {
      this->Sum = this->BinaryOperation(left.Sum, this->Sum);
    }
  }

  VTKM_SUPPRESS_EXEC_WARNINGS
  VTKM_EXEC_CONT
  void assign(const ScanExclusiveBody& src) { this->Sum = src.Sum; }
};

VTKM_SUPPRESS_EXEC_WARNINGS
template <class InputPortalType, class OutputPortalType, class BinaryOperationType>
VTKM_CONT static typename std::remove_reference<typename OutputPortalType::ValueType>::type
ScanInclusivePortals(InputPortalType inputPortal,
                     OutputPortalType outputPortal,
                     BinaryOperationType binaryOperation)
{
  using ValueType = typename std::remove_reference<typename OutputPortalType::ValueType>::type;

  using WrappedBinaryOp = internal::WrappedBinaryOperator<ValueType, BinaryOperationType>;

  WrappedBinaryOp wrappedBinaryOp(binaryOperation);
  ScanInclusiveBody<InputPortalType, OutputPortalType, WrappedBinaryOp> body(
    inputPortal, outputPortal, wrappedBinaryOp);
  vtkm::Id arrayLength = inputPortal.GetNumberOfValues();

  ::tbb::blocked_range<vtkm::Id> range(0, arrayLength, TBB_GRAIN_SIZE);
  ::tbb::parallel_scan(range, body);
  return body.Sum;
}

VTKM_SUPPRESS_EXEC_WARNINGS
template <class InputPortalType, class OutputPortalType, class BinaryOperationType>
VTKM_CONT static typename std::remove_reference<typename OutputPortalType::ValueType>::type
ScanExclusivePortals(
  InputPortalType inputPortal,
  OutputPortalType outputPortal,
  BinaryOperationType binaryOperation,
  typename std::remove_reference<typename OutputPortalType::ValueType>::type initialValue)
{
  using ValueType = typename std::remove_reference<typename OutputPortalType::ValueType>::type;

  using WrappedBinaryOp = internal::WrappedBinaryOperator<ValueType, BinaryOperationType>;

  WrappedBinaryOp wrappedBinaryOp(binaryOperation);
  ScanExclusiveBody<InputPortalType, OutputPortalType, WrappedBinaryOp> body(
    inputPortal, outputPortal, wrappedBinaryOp, initialValue);
  vtkm::Id arrayLength = inputPortal.GetNumberOfValues();

  ::tbb::blocked_range<vtkm::Id> range(0, arrayLength, TBB_GRAIN_SIZE);
  ::tbb::parallel_scan(range, body);

  // Seems a little weird to me that we would return the last value in the
  // array rather than the sum, but that is how the function is specified.
  return body.Sum;
}

template <typename InputPortalType, typename IndexPortalType, typename OutputPortalType>
class ScatterKernel
{
public:
  VTKM_CONT ScatterKernel(InputPortalType inputPortal,
                          IndexPortalType indexPortal,
                          OutputPortalType outputPortal)
    : ValuesPortal(inputPortal)
    , IndexPortal(indexPortal)
    , OutputPortal(outputPortal)
  {
  }

  VTKM_CONT
  void operator()(const ::tbb::blocked_range<vtkm::Id>& range) const
  {
    // The TBB device adapter causes array classes to be shared between
    // control and execution environment. This means that it is possible for
    // an exception to be thrown even though this is typically not allowed.
    // Throwing an exception from here is bad because there are several
    // simultaneous threads running. Get around the problem by catching the
    // error and setting the message buffer as expected.
    try
    {
      VTKM_VECTORIZATION_PRE_LOOP
      for (vtkm::Id i = range.begin(); i < range.end(); i++)
      {
        VTKM_VECTORIZATION_IN_LOOP
        OutputPortal.Set(i, ValuesPortal.Get(IndexPortal.Get(i)));
      }
    }
    catch (vtkm::cont::Error& error)
    {
      this->ErrorMessage.RaiseError(error.GetMessage().c_str());
    }
    catch (...)
    {
      this->ErrorMessage.RaiseError("Unexpected error in execution environment.");
    }
  }

private:
  InputPortalType ValuesPortal;
  IndexPortalType IndexPortal;
  OutputPortalType OutputPortal;
  vtkm::exec::internal::ErrorMessageBuffer ErrorMessage;
};

VTKM_SUPPRESS_EXEC_WARNINGS
template <typename InputPortalType, typename IndexPortalType, typename OutputPortalType>
VTKM_CONT static void ScatterPortal(InputPortalType inputPortal,
                                    IndexPortalType indexPortal,
                                    OutputPortalType outputPortal)
{
  const vtkm::Id size = inputPortal.GetNumberOfValues();
  VTKM_ASSERT(size == indexPortal.GetNumberOfValues());

  ScatterKernel<InputPortalType, IndexPortalType, OutputPortalType> scatter(
    inputPortal, indexPortal, outputPortal);

  ::tbb::blocked_range<vtkm::Id> range(0, size, TBB_GRAIN_SIZE);
  ::tbb::parallel_for(range, scatter);
}
}
}
}
#endif //vtk_m_cont_tbb_internal_FunctorsTBB_h
