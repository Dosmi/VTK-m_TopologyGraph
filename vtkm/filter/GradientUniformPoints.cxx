//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#ifndef vtk_m_filter_Gradient_cxx
#define vtk_m_filter_Gradient_cxx

#include <vtkm/filter/Gradient.h>

namespace vtkm
{
namespace filter
{
template VTKM_FILTER_GRADIENT_EXPORT vtkm::cont::DataSet Gradient::DoExecute(
  const vtkm::cont::DataSet&,
  const vtkm::cont::ArrayHandle<vtkm::Vec3f, vtkm::cont::StorageTagUniformPoints>&,
  const vtkm::filter::FieldMetadata&,
  vtkm::filter::PolicyBase<vtkm::filter::PolicyDefault>);
}
}

#endif
