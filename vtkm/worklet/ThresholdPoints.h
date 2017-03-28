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
#ifndef vtkm_m_worklet_ThresholdPoints_h
#define vtkm_m_worklet_ThresholdPoints_h

#include <vtkm/worklet/DispatcherMapTopology.h>
#include <vtkm/worklet/WorkletMapTopology.h>
#include <vtkm/worklet/WorkletMapField.h>

#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/ArrayHandle.h>
#include <vtkm/cont/DeviceAdapterAlgorithm.h>

namespace vtkm {
namespace worklet {

// Threshold points on predicate producing CellSetSingleType<VERTEX> with
// resulting subset of points
class ThresholdPoints : public vtkm::worklet::WorkletMapPointToCell
{
public:
  struct BoolType : vtkm::ListTagBase<bool> { };

  template <typename UnaryPredicate>
  class ThresholdPointField : public vtkm::worklet::WorkletMapField
  {
  public:
    typedef void ControlSignature(FieldIn<> scalars,
                                  FieldOut<BoolType> passFlags);
    typedef _2 ExecutionSignature(_1);

    VTKM_CONT
    ThresholdPointField() : Predicate() { }

    VTKM_CONT
    explicit ThresholdPointField(const UnaryPredicate &predicate)
      : Predicate(predicate)
    { }

    template<typename ScalarType>
    VTKM_EXEC
    bool operator()(const ScalarType &scalar) const
    {
      return this->Predicate(scalar);
    }

  private:
    UnaryPredicate Predicate;
  };

  template <typename CellSetType, 
            typename UnaryPredicate, 
            typename ValueType, 
            typename StorageType, 
            typename DeviceAdapter>
  vtkm::cont::CellSetSingleType<> Run(
                          const CellSetType &cellSet,
                          const vtkm::cont::ArrayHandle<ValueType, StorageType>& fieldArray,
                          const UnaryPredicate &predicate,
                          DeviceAdapter device)
  {
    typedef typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapter> DeviceAlgorithm;

    vtkm::cont::ArrayHandle<bool> passFlags;
    vtkm::Id numberOfInputPoints = cellSet.GetNumberOfPoints();
    
    // Worklet output will be a boolean passFlag array
    typedef ThresholdPointField<UnaryPredicate> ThresholdWorklet;
    ThresholdWorklet worklet(predicate);
    DispatcherMapField<ThresholdWorklet, DeviceAdapter> dispatcher(worklet);
    dispatcher.Invoke(fieldArray, passFlags);

    vtkm::cont::ArrayHandle<vtkm::Id> pointIds;
    vtkm::cont::ArrayHandleCounting<vtkm::Id> indices =
      vtkm::cont::make_ArrayHandleCounting(vtkm::Id(0), vtkm::Id(1), passFlags.GetNumberOfValues());
    DeviceAlgorithm::CopyIf(indices, passFlags, pointIds);

    // Make CellSetSingleType with VERTEX at each point id
    vtkm::cont::CellSetSingleType< > outCellSet(cellSet.GetName());
    outCellSet.Fill(numberOfInputPoints,
                    vtkm::CellShapeTagVertex::Id,
                    1,
                    pointIds);

    return outCellSet;
  }
};

}
} // namespace vtkm::worklet

#endif // vtkm_m_worklet_ThresholdPoints_h
