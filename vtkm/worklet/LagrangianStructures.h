//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================
#ifndef vtk_m_worklet_LagrangianStructures_h
#define vtk_m_worklet_LagrangianStructures_h

#include <vtkm/Matrix.h>
#include <vtkm/Types.h>
#include <vtkm/worklet/DispatcherMapField.h>
#include <vtkm/worklet/WorkletMapField.h>

#include <vtkm/worklet/lcs/LagrangianStructureHelpers.h>

namespace vtkm
{
namespace worklet
{

class GridMetaData
{
public:
  using Structured2DType = vtkm::cont::CellSetStructured<2>;
  using Structured3DType = vtkm::cont::CellSetStructured<3>;

  VTKM_CONT
  GridMetaData(const vtkm::cont::DynamicCellSet cellSet)
  {
    if (cellSet.IsType<Structured2DType>())
    {
      this->cellSet2D = true;
      vtkm::Id2 dims =
        cellSet.Cast<Structured2DType>().GetSchedulingRange(vtkm::TopologyElementTagPoint());
      this->Dims = vtkm::Id3(dims[0], dims[1], 1);
    }
    else
    {
      this->cellSet2D = false;
      this->Dims =
        cellSet.Cast<Structured3DType>().GetSchedulingRange(vtkm::TopologyElementTagPoint());
    }
    this->PlaneSize = Dims[0] * Dims[1];
    this->RowSize = Dims[0];
  }

  VTKM_EXEC
  bool IsCellSet2D() const { return this->cellSet2D; }

  VTKM_EXEC
  void GetLogicalIndex(const vtkm::Id index, vtkm::Id3& logicalIndex) const
  {
    logicalIndex[0] = index % Dims[0];
    logicalIndex[1] = (index / Dims[0]) % Dims[1];
    if (this->cellSet2D)
      logicalIndex[2] = 0;
    else
      logicalIndex[2] = index / (Dims[0] * Dims[1]);
  }

  VTKM_EXEC
  const vtkm::Vec<vtkm::Id, 6> GetNeighborIndices(const vtkm::Id index) const
  {
    vtkm::Vec<vtkm::Id, 6> indices;
    vtkm::Id3 logicalIndex;
    GetLogicalIndex(index, logicalIndex);

    // For differentials w.r.t delta in x
    indices[0] = (logicalIndex[0] == 0) ? index : index - 1;
    indices[1] = (logicalIndex[0] == Dims[0] - 1) ? index : index + 1;
    // For differentials w.r.t delta in y
    indices[2] = (logicalIndex[1] == 0) ? index : index - RowSize;
    indices[3] = (logicalIndex[1] == Dims[1] - 1) ? index : index + RowSize;
    if (!this->cellSet2D)
    {
      // For differentials w.r.t delta in z
      indices[4] = (logicalIndex[2] == 0) ? index : index - PlaneSize;
      indices[5] = (logicalIndex[2] == Dims[2] - 1) ? index : index + PlaneSize;
    }
    return indices;
  }

private:
  bool cellSet2D = false;
  vtkm::Id3 Dims;
  vtkm::Id PlaneSize;
  vtkm::Id RowSize;
};

template <vtkm::IdComponent dimensions>
class LagrangianStructures;

template <>
class LagrangianStructures<2> : public vtkm::worklet::WorkletMapField
{
public:
  using Scalar = vtkm::FloatDefault;

  VTKM_CONT
  LagrangianStructures(Scalar endTime, vtkm::cont::DynamicCellSet cellSet)
    : EndTime(endTime)
    , GridData(cellSet)
  {
  }

  using ControlSignature = void(WholeArrayIn, WholeArrayIn, FieldOut);

  using ExecutionSignature = void(WorkIndex, _1, _2, _3);

  template <typename PointArray>
  VTKM_EXEC void operator()(const vtkm::Id index,
                            const PointArray& input,
                            const PointArray& output,
                            Scalar& outputField) const
  {
    using Point = typename PointArray::ValueType;

    const vtkm::Vec<vtkm::Id, 6> neighborIndices = this->GridData.GetNeighborIndices(index);

    // Calculate Stretching / Squeezing
    Point xin1 = input.Get(neighborIndices[0]);
    Point xin2 = input.Get(neighborIndices[1]);
    Point yin1 = input.Get(neighborIndices[2]);
    Point yin2 = input.Get(neighborIndices[3]);

    Scalar xDiff = 1.0f / (xin2[0] - xin1[0]);
    Scalar yDiff = 1.0f / (yin2[1] - yin1[1]);

    Point xout1 = output.Get(neighborIndices[0]);
    Point xout2 = output.Get(neighborIndices[1]);
    Point yout1 = output.Get(neighborIndices[2]);
    Point yout2 = output.Get(neighborIndices[3]);

    // Total X gradient w.r.t X, Y
    Scalar f1x = (xout2[0] - xout1[0]) * xDiff;
    Scalar f1y = (yout2[0] - yout1[0]) * yDiff;

    // Total Y gradient w.r.t X, Y
    Scalar f2x = (xout2[1] - xout1[1]) * xDiff;
    Scalar f2y = (yout2[1] - yout1[1]) * yDiff;

    vtkm::Matrix<Scalar, 2, 2> jacobian;
    vtkm::MatrixSetRow(jacobian, 0, vtkm::Vec<Scalar, 2>(f1x, f1y));
    vtkm::MatrixSetRow(jacobian, 1, vtkm::Vec<Scalar, 2>(f2x, f2y));

    detail::ComputeLeftCauchyGreenTensor(jacobian);

    vtkm::Vec<Scalar, 2> eigenValues;
    detail::Jacobi(jacobian, eigenValues);

    Scalar delta = eigenValues[0];
    // Check if we need to clamp these values
    // Also provide options.
    // 1. FTLE
    // 2. FLLE
    // 3. Eigen Values (Min/Max)
    //Scalar delta = trace + sqrtr;
    // Given endTime is in units where start time is 0,
    // else do endTime-startTime
    // return value for computation
    outputField = log(delta) / (2 * EndTime);
  }

public:
  // To calculate FTLE field
  Scalar EndTime;
  // To assist in calculation of indices
  GridMetaData GridData;
};

template <>
class LagrangianStructures<3> : public vtkm::worklet::WorkletMapField
{
public:
  using Scalar = vtkm::FloatDefault;

  VTKM_CONT
  LagrangianStructures(Scalar endTime, vtkm::cont::DynamicCellSet cellSet)
    : EndTime(endTime)
    , GridData(cellSet)
  {
  }

  using ControlSignature = void(WholeArrayIn, WholeArrayIn, FieldOut);

  using ExecutionSignature = void(WorkIndex, _1, _2, _3);

  /*
   * Point position arrays are the input and the output positions of the particle advection.
   */
  template <typename PointArray>
  VTKM_EXEC void operator()(const vtkm::Id index,
                            const PointArray& input,
                            const PointArray& output,
                            Scalar& outputField) const
  {
    using Point = typename PointArray::ValueType;

    const vtkm::Vec<vtkm::Id, 6> neighborIndices = this->GridData.GetNeighborIndices(index);

    Point xin1 = input.Get(neighborIndices[0]);
    Point xin2 = input.Get(neighborIndices[1]);
    Point yin1 = input.Get(neighborIndices[2]);
    Point yin2 = input.Get(neighborIndices[3]);
    Point zin1 = input.Get(neighborIndices[4]);
    Point zin2 = input.Get(neighborIndices[5]);

    Scalar xDiff = 1.0f / (xin2[0] - xin1[0]);
    Scalar yDiff = 1.0f / (yin2[1] - yin1[1]);
    Scalar zDiff = 1.0f / (zin2[2] - zin1[2]);

    Point xout1 = output.Get(neighborIndices[0]);
    Point xout2 = output.Get(neighborIndices[1]);
    Point yout1 = output.Get(neighborIndices[2]);
    Point yout2 = output.Get(neighborIndices[3]);
    Point zout1 = output.Get(neighborIndices[4]);
    Point zout2 = output.Get(neighborIndices[5]);

    // Total X gradient w.r.t X, Y, Z
    Scalar f1x = (xout2[0] - xout1[0]) * xDiff;
    Scalar f1y = (yout2[0] - yout1[0]) * yDiff;
    Scalar f1z = (zout2[0] - zout1[0]) * zDiff;

    // Total Y gradient w.r.t X, Y, Z
    Scalar f2x = (xout2[1] - xout1[1]) * xDiff;
    Scalar f2y = (yout2[1] - yout1[1]) * yDiff;
    Scalar f2z = (zout2[1] - zout1[1]) * zDiff;

    // Total Z gradient w.r.t X, Y, Z
    Scalar f3x = (xout2[2] - xout1[2]) * xDiff;
    Scalar f3y = (yout2[2] - yout1[2]) * yDiff;
    Scalar f3z = (zout2[2] - zout1[2]) * zDiff;

    vtkm::Matrix<Scalar, 3, 3> jacobian;
    vtkm::MatrixSetRow(jacobian, 0, vtkm::Vec<Scalar, 3>(f1x, f1y, f1z));
    vtkm::MatrixSetRow(jacobian, 1, vtkm::Vec<Scalar, 3>(f2x, f2y, f2z));
    vtkm::MatrixSetRow(jacobian, 2, vtkm::Vec<Scalar, 3>(f3x, f3y, f3z));

    detail::ComputeLeftCauchyGreenTensor(jacobian);

    vtkm::Vec<Scalar, 3> eigenValues;
    detail::Jacobi(jacobian, eigenValues);

    Scalar delta = eigenValues[0];
    if (delta == 0.0)
    {
      outputField = 0;
    }
    // Given endTime is in units where start time is 0. else do endTime-startTime
    // return value for ftle computation
    outputField = log(delta) / (2 * EndTime);
  }

public:
  // To calculate FTLE field
  Scalar EndTime;
  // To assist in calculation of indices
  GridMetaData GridData;
};

} // worklet
} // vtkm

#endif //vtk_m_worklet_LagrangianStructures_h
