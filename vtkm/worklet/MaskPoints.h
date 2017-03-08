//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2015 Sandia Corporation.
//  Copyright 2015 UT-Battelle, LLC.
//  Copyright 2015 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#ifndef vtkm_m_worklet_MaskPoints_h
#define vtkm_m_worklet_MaskPoints_h

#include <vtkm/worklet/DispatcherMapTopology.h>
#include <vtkm/worklet/WorkletMapTopology.h>

#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/ArrayHandle.h>
#include <vtkm/cont/ArrayHandleCounting.h>
#include <vtkm/cont/DeviceAdapterAlgorithm.h>

namespace vtkm {
namespace worklet {

// Subselect points using stride for now, creating new cellset of vertices
class MaskPoints : public vtkm::worklet::WorkletMapPointToCell
{
public:
  template <typename CellSetType, 
            typename DeviceAdapter>
  vtkm::cont::CellSetSingleType<> Run(
                          const CellSetType &cellSet,
                          const vtkm::Id stride,
                          DeviceAdapter device)
  {
    typedef typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapter> DeviceAlgorithm;
    vtkm::Id numberOfInputPoints = cellSet.GetNumberOfPoints();
    vtkm::Id numberOfSampledPoints = numberOfInputPoints / stride;
    vtkm::cont::ArrayHandleCounting<vtkm::Id> strideArray(0, stride, numberOfSampledPoints);

    vtkm::cont::ArrayHandle<vtkm::Id> pointIds;
    DeviceAlgorithm::Copy(strideArray, pointIds);
    printSummary_ArrayHandle(pointIds, std::cout);

    // Make CellSetSingleType with VERTEX at each point id
    vtkm::cont::CellSetSingleType< > outCellSet("cells");
    outCellSet.Fill(numberOfInputPoints,
                    vtkm::CellShapeTagVertex::Id,
                    1,
                    pointIds);

    return outCellSet;
  }
};

}
} // namespace vtkm::worklet

#endif // vtkm_m_worklet_MaskPoints_h
