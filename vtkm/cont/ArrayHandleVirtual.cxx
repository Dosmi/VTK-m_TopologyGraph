//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2019 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
//  Copyright 2019 UT-Battelle, LLC.
//  Copyright 2019 Los Alamos National Security.
//
//  Under the terms of Contract DE-NA0003525 with NTESS,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#define vtk_m_cont_ArrayHandleVirtual_cxx
#include <vtkm/cont/ArrayHandleVirtual.h>

namespace vtkm
{
namespace cont
{

#define VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(T)                                                  \
  template class VTKM_CONT_EXPORT ArrayHandle<T, StorageTagVirtual>;                               \
  template class VTKM_CONT_EXPORT ArrayHandleVirtual<T>;                                           \
  template class VTKM_CONT_EXPORT ArrayHandle<vtkm::Vec<T, 2>, StorageTagVirtual>;                 \
  template class VTKM_CONT_EXPORT ArrayHandleVirtual<vtkm::Vec<T, 2>>;                             \
  template class VTKM_CONT_EXPORT ArrayHandle<vtkm::Vec<T, 3>, StorageTagVirtual>;                 \
  template class VTKM_CONT_EXPORT ArrayHandleVirtual<vtkm::Vec<T, 3>>;                             \
  template class VTKM_CONT_EXPORT ArrayHandle<vtkm::Vec<T, 4>, StorageTagVirtual>;                 \
  template class VTKM_CONT_EXPORT ArrayHandleVirtual<vtkm::Vec<T, 4>>

VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(char);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::Int8);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::UInt8);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::Int16);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::UInt16);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::Int32);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::UInt32);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::Int64);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::UInt64);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::Float32);
VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE(vtkm::Float64);

#undef VTK_M_ARRAY_HANDLE_VIRTUAL_INSTANTIATE
}
} //namespace vtkm::cont