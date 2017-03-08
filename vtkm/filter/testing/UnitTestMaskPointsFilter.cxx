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

#include <vtkm/cont/testing/MakeTestDataSet.h>
#include <vtkm/cont/testing/Testing.h>

#include <vtkm/filter/MaskPoints.h>

using vtkm::cont::testing::MakeTestDataSet;

namespace {


class TestingMaskPoints
{
public:
  void TestRegular2D() const
  {
    std::cout << "Testing mask points on 2D regular dataset" << std::endl;
    vtkm::cont::DataSet dataset = MakeTestDataSet().Make2DUniformDataSet1();
    vtkm::filter::ResultDataSet result;

    vtkm::filter::MaskPoints maskPoints;
    maskPoints.SetStride(2);
    maskPoints.SetCompactPoints(true);

    result = maskPoints.Execute(dataset);

    maskPoints.MapFieldOntoOutput(result, dataset.GetField("pointvar") );

    vtkm::cont::DataSet output = result.GetDataSet();
    VTKM_TEST_ASSERT(test_equal(output.GetCellSet().GetNumberOfCells(), 12), 
                     "Wrong result for MaskPoints");
  }

  void TestRegular3D() const
  {
    std::cout << "Testing mask points on 3D regular dataset" << std::endl;
    vtkm::cont::DataSet dataset = MakeTestDataSet().Make3DUniformDataSet1();
    vtkm::filter::ResultDataSet result;

    vtkm::filter::MaskPoints maskPoints;
    maskPoints.SetStride(5);
    result = maskPoints.Execute(dataset);

    maskPoints.MapFieldOntoOutput(result, dataset.GetField("pointvar") );

    vtkm::cont::DataSet output = result.GetDataSet();
    VTKM_TEST_ASSERT(test_equal(output.GetCellSet().GetNumberOfCells(), 25), 
                     "Wrong result for MaskPoints");
  }

  void TestExplicit3D() const
  {
    std::cout << "Testing mask points on 3D explicit dataset" << std::endl;
    vtkm::cont::DataSet dataset = MakeTestDataSet().Make3DExplicitDataSet5();
    vtkm::filter::ResultDataSet result;

    vtkm::filter::MaskPoints maskPoints;
    maskPoints.SetStride(3);
    result = maskPoints.Execute(dataset);

    maskPoints.MapFieldOntoOutput(result, dataset.GetField("pointvar") );

    vtkm::cont::DataSet output = result.GetDataSet();
    VTKM_TEST_ASSERT(test_equal(output.GetCellSet().GetNumberOfCells(), 3), 
                     "Wrong result for MaskPoints");
  }

  void operator()() const
  {
    this->TestRegular2D();
    this->TestRegular3D();
    this->TestExplicit3D();
  }
};

}

int UnitTestMaskPointsFilter(int, char *[])
{
  return vtkm::cont::testing::Testing::Run(TestingMaskPoints());
}
