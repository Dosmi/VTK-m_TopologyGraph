//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================
#ifndef vtk_m_filter_ZFPDecompressor2D_h
#define vtk_m_filter_ZFPDecompressor2D_h

#include <vtkm/Deprecated.h>
#include <vtkm/filter/zfp/ZFPDecompressor2D.h>

namespace vtkm
{
namespace filter
{

VTKM_DEPRECATED(
  1.8,
  "Use vtkm/filter/zfp/ZFPDecompressor2D.h instead of vtkm/filter/ZFPDecompressor2D.h.")
inline void ZFPDecompressor2D_deprecated() {}

inline void ZFPDecompressor2D_deprecated_warning()
{
  ZFPDecompressor2D_deprecated();
}

}
} // namespace vtkm::filter

#endif //vtk_m_filter_ZFPDecompressor2D_h
