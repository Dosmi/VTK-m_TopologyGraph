//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include <vtkm/Math.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/testing/Testing.h>
#include <vtkm/filter/CleanGrid.h>

#include <vtkm/filter/Contour.h>
#include <vtkm/io/VTKDataSetReader.h>
#include <vtkm/source/Tangle.h>

namespace
{

class TestContourFilter
{
public:
  void TestContourUniformGrid() const
  {
    std::cout << "Testing Contour filter on a uniform grid" << std::endl;

    vtkm::Id3 dims(4, 4, 4);
    vtkm::source::Tangle tangle(dims);
    vtkm::cont::DataSet dataSet = tangle.Execute();

    vtkm::filter::Contour mc;

    mc.SetGenerateNormals(true);
    mc.SetIsoValue(0, 0.5);
    mc.SetActiveField("nodevar");
    mc.SetFieldsToPass(vtkm::filter::FieldSelection::MODE_NONE);

    auto result = mc.Execute(dataSet);
    {
      VTKM_TEST_ASSERT(result.GetNumberOfCoordinateSystems() == 1,
                       "Wrong number of coordinate systems in the output dataset");
      //since normals is on we have one field
      VTKM_TEST_ASSERT(result.GetNumberOfFields() == 1,
                       "Wrong number of fields in the output dataset");
    }

    // let's execute with mapping fields.
    mc.SetFieldsToPass("nodevar");
    result = mc.Execute(dataSet);
    {
      const bool isMapped = result.HasField("nodevar");
      VTKM_TEST_ASSERT(isMapped, "mapping should pass");

      VTKM_TEST_ASSERT(result.GetNumberOfFields() == 2,
                       "Wrong number of fields in the output dataset");

      vtkm::cont::CoordinateSystem coords = result.GetCoordinateSystem();
      vtkm::cont::DynamicCellSet dcells = result.GetCellSet();
      using CellSetType = vtkm::cont::CellSetSingleType<>;
      const CellSetType& cells = dcells.Cast<CellSetType>();

      //verify that the number of points is correct (72)
      //verify that the number of cells is correct (160)
      VTKM_TEST_ASSERT(coords.GetNumberOfPoints() == 72,
                       "Should have less coordinates than the unmerged version");
      VTKM_TEST_ASSERT(cells.GetNumberOfCells() == 160, "");
    }

    //Now try with vertex merging disabled. Since this
    //we use FlyingEdges we now which does point merging for free
    //so we should see the number of points not change
    mc.SetMergeDuplicatePoints(false);
    mc.SetFieldsToPass(vtkm::filter::FieldSelection::MODE_ALL);
    result = mc.Execute(dataSet);
    {
      vtkm::cont::CoordinateSystem coords = result.GetCoordinateSystem();

      VTKM_TEST_ASSERT(coords.GetNumberOfPoints() == 72,
                       "Shouldn't have less coordinates than the unmerged version");

      //verify that the number of cells is correct (160)
      vtkm::cont::DynamicCellSet dcells = result.GetCellSet();

      using CellSetType = vtkm::cont::CellSetSingleType<>;
      const CellSetType& cells = dcells.Cast<CellSetType>();
      VTKM_TEST_ASSERT(cells.GetNumberOfCells() == 160, "");
    }
  }

  void TestContourWedges() const
  {
    auto pathname =
      vtkm::cont::testing::Testing::GetTestDataBasePath() + "/unstructured/wedge_cells.vtk";
    vtkm::io::VTKDataSetReader reader(pathname);

    vtkm::cont::DataSet dataSet = reader.ReadDataSet();

    vtkm::cont::CellSetExplicit<> cellSet;
    dataSet.GetCellSet().CopyTo(cellSet);

    vtkm::cont::ArrayHandle<vtkm::Float32> fieldArray;
    dataSet.GetPointField("gyroid").GetData().CopyTo(fieldArray);

    vtkm::worklet::Contour isosurfaceFilter;
    isosurfaceFilter.SetMergeDuplicatePoints(false);

    vtkm::cont::ArrayHandle<vtkm::Vec3f_32> verticesArray;
    vtkm::cont::ArrayHandle<vtkm::Vec3f_32> normalsArray;

    auto result = isosurfaceFilter.Run(
      { 0.0f }, cellSet, dataSet.GetCoordinateSystem(), fieldArray, verticesArray, normalsArray);
    VTKM_TEST_ASSERT(result.GetNumberOfCells() == 52);
  }

  void operator()() const
  {
    this->TestContourUniformGrid();
    this->TestContourWedges();
  }

}; // class TestContourFilter
} // namespace

int UnitTestContourFilter(int argc, char* argv[])
{
  return vtkm::cont::testing::Testing::Run(TestContourFilter{}, argc, argv);
}
