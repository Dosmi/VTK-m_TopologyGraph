//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#ifndef vtk_m_filter_mesh_info_CellMeasure_h
#define vtk_m_filter_mesh_info_CellMeasure_h

#include <vtkm/filter/NewFilterField.h>
#include <vtkm/filter/mesh_info/vtkm_filter_mesh_info_export.h>

namespace vtkm
{
namespace filter
{
namespace mesh_info
{
enum IntegrationType
{
  ArcLength = 0x01,
  Area = 0x02,
  Volume = 0x04,
  AllMeasures = ArcLength | Area | Volume
};

/// \brief Compute the measure of each (3D) cell in a dataset.
///
/// CellMeasures is a filter that generates a new cell data array (i.e., one value
/// specified per cell) holding the signed measure of the cell
/// or 0 (if measure is not well defined or the cell type is unsupported).
///
/// By default, the new cell-data array is named "measure".
class VTKM_FILTER_MESH_INFO_EXPORT CellMeasures : public vtkm::filter::NewFilterField
{
public:
  VTKM_CONT
  explicit CellMeasures(IntegrationType);

  /// Set/Get the name of the cell measure field. If not set, "measure" is used.
  void SetCellMeasureName(const std::string& name) { this->SetOutputFieldName(name); }
  const std::string& GetCellMeasureName() const { return this->GetOutputFieldName(); }

private:
  VTKM_CONT vtkm::cont::DataSet DoExecute(const vtkm::cont::DataSet& input) override;

  IntegrationType measure;
};
} // namespace mesh_info
} // namespace filter
} // namespace vtkm

#endif // vtk_m_filter_mesh_info_CellMeasure_h