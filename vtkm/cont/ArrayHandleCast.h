//=============================================================================
//
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2015 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
//  Copyright 2015 UT-Battelle, LLC.
//  Copyright 2015 Los Alamos National Security.
//
//  Under the terms of Contract DE-NA0003525 with NTESS,
//  the U.S. Government retains certain rights in this software.
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//
//=============================================================================
#ifndef vtk_m_cont_ArrayHandleCast_h
#define vtk_m_cont_ArrayHandleCast_h

#include <vtkm/cont/ArrayHandleTransform.h>

namespace vtkm
{
namespace cont
{

namespace internal
{

template <typename FromType, typename ToType>
struct VTKM_ALWAYS_EXPORT Cast
{
  VTKM_EXEC_CONT
  ToType operator()(const FromType& val) const { return static_cast<ToType>(val); }
};

} // namespace internal

/// \brief Cast the values of an array to the specified type, on demand.
///
/// ArrayHandleCast is a specialization of ArrayHandleTransform. Given an ArrayHandle
/// and a type, it creates a new handle that returns the elements of the array cast
/// to the specified type.
///
template <typename T, typename ArrayHandleType>
class ArrayHandleCast
  : public vtkm::cont::ArrayHandleTransform<ArrayHandleType,
                                            internal::Cast<typename ArrayHandleType::ValueType, T>,
                                            internal::Cast<T, typename ArrayHandleType::ValueType>>
{
public:
  VTKM_ARRAY_HANDLE_SUBCLASS(
    ArrayHandleCast,
    (ArrayHandleCast<T, ArrayHandleType>),
    (vtkm::cont::ArrayHandleTransform<ArrayHandleType,
                                      internal::Cast<typename ArrayHandleType::ValueType, T>,
                                      internal::Cast<T, typename ArrayHandleType::ValueType>>));

  ArrayHandleCast(const ArrayHandleType& handle)
    : Superclass(handle)
  {
  }
};

/// make_ArrayHandleCast is convenience function to generate an
/// ArrayHandleCast.
///
template <typename T, typename HandleType>
VTKM_CONT ArrayHandleCast<T, HandleType> make_ArrayHandleCast(const HandleType& handle,
                                                              const T& = T())
{
  return ArrayHandleCast<T, HandleType>(handle);
}

/// Overriden version of \c make_ArrayHandleCast that returns the original object type.
///
template <typename T, typename Storage>
VTKM_CONT vtkm::cont::ArrayHandle<T, Storage> make_ArrayHandleCast(
  const vtkm::cont::ArrayHandle<T, Storage>& handle,
  const T& = T())
{
  return handle;
}
}
} // namespace vtkm::cont

#endif // vtk_m_cont_ArrayHandleCast_h
