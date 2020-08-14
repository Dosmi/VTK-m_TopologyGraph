//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================
#ifndef vtk_m_filter_Streamline_hxx
#define vtk_m_filter_Streamline_hxx

#include <vtkm/filter/Streamline.h>

#include <vtkm/cont/ArrayCopy.h>
#include <vtkm/cont/ArrayHandleIndex.h>
#include <vtkm/cont/ErrorFilterExecution.h>
#include <vtkm/worklet/particleadvection/Field.h>
#include <vtkm/worklet/particleadvection/GridEvaluators.h>
#include <vtkm/worklet/particleadvection/IntegratorBase.h>
#include <vtkm/worklet/particleadvection/Particles.h>

namespace vtkm
{
namespace filter
{

//-----------------------------------------------------------------------------
inline VTKM_CONT Streamline::Streamline()
  : vtkm::filter::FilterDataSetWithField<Streamline>()
  , Worklet()
{
}

//-----------------------------------------------------------------------------
inline VTKM_CONT void Streamline::SetSeeds(vtkm::cont::ArrayHandle<vtkm::Massless>& seeds)
{
  this->Seeds = seeds;
}

//-----------------------------------------------------------------------------
template <typename T, typename StorageType, typename DerivedPolicy>
inline VTKM_CONT vtkm::cont::DataSet Streamline::DoExecute(
  const vtkm::cont::DataSet& input,
  const vtkm::cont::ArrayHandle<vtkm::Vec<T, 3>, StorageType>& field,
  const vtkm::filter::FieldMetadata& fieldMeta,
  const vtkm::filter::PolicyBase<DerivedPolicy>&)
{
  //Check for some basics.
  if (this->Seeds.GetNumberOfValues() == 0)
  {
    throw vtkm::cont::ErrorFilterExecution("No seeds provided.");
  }

  const vtkm::cont::DynamicCellSet& cells = input.GetCellSet();
  const vtkm::cont::CoordinateSystem& coords =
    input.GetCoordinateSystem(this->GetActiveCoordinateSystemIndex());

  if (!fieldMeta.IsPointField())
  {
    throw vtkm::cont::ErrorFilterExecution("Point field expected.");
  }

  using FieldHandle = vtkm::cont::ArrayHandle<vtkm::Vec<T, 3>, StorageType>;
  using FieldType = vtkm::worklet::particleadvection::VelocityField<FieldHandle>;
  using GridEvalType = vtkm::worklet::particleadvection::GridEvaluator<FieldType>;
  using RK4Type = vtkm::worklet::particleadvection::RK4Integrator<GridEvalType>;

  FieldType velocities(field);
  GridEvalType eval(coords, cells, velocities);
  RK4Type rk4(eval, this->StepSize);

  vtkm::worklet::StreamlineResult<vtkm::Massless> res;

  vtkm::cont::ArrayHandle<vtkm::Massless> seedArray;
  vtkm::cont::ArrayCopy(this->Seeds, seedArray);
  res = this->Worklet.Run(rk4, seedArray, this->NumberOfSteps);

  vtkm::cont::DataSet outData;
  vtkm::cont::CoordinateSystem outputCoords("coordinates", res.Positions);
  outData.SetCellSet(res.PolyLines);
  outData.AddCoordinateSystem(outputCoords);

  return outData;
}

//-----------------------------------------------------------------------------
template <typename DerivedPolicy>
inline VTKM_CONT bool Streamline::MapFieldOntoOutput(vtkm::cont::DataSet&,
                                                     const vtkm::cont::Field&,
                                                     vtkm::filter::PolicyBase<DerivedPolicy>)
{
  return false;
}
}
} // namespace vtkm::filter
#endif
