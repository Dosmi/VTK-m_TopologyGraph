//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2014 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
//  Copyright 2014 UT-Battelle, LLC.
//  Copyright 2014 Los Alamos National Security.
//
//  Under the terms of Contract DE-NA0003525 with NTESS,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#ifndef vtk_m_worklet_cellmetrics_CellRelativeSizeSquaredMetric_h
#define vtk_m_worklet_cellmetrics_CellRelativeSizeSquaredMetric_h

/*
 * Mesh quality metric functions that compute the relative size squared of mesh
 * cells. The RSS of a cell is defined as the square of the minimum of: the area
 * divided by the average area of an ensemble of triangles or the inverse. For
 * 3D cells we use the volumes instead of the areas. 
 *
 * These metric computations are adapted from the VTK implementation of the
 * Verdict library, which provides a set of mesh/cell metrics for evaluating the
 * geometric qualities of regions of mesh spaces.
 *
 * See: The Verdict Library Reference Manual (for per-cell-type metric formulae)
 * See: vtk/ThirdParty/verdict/vtkverdict (for VTK code implementation of this
 * metric)
 */

#include "vtkm/CellShape.h"
#include "vtkm/CellTraits.h"
#include "vtkm/VecTraits.h"
#include "vtkm/VectorAnalysis.h"
#include "vtkm/exec/FunctorBase.h"

#define UNUSED(expr) (void)(expr);

namespace vtkm
{
namespace worklet
{
namespace cellmetrics
{

using FloatType = vtkm::FloatDefault;

// ========================= Unsupported cells ==================================

// By default, cells have zero shape unless the shape type template is specialized below.
template <typename OutType, typename PointCoordVecType, typename CellShapeType>
VTKM_EXEC OutType CellRelativeSizeSquaredMetric(const vtkm::IdComponent& numPts,
                                                const PointCoordVecType& pts,
                                                const OutType& avgArea,
                                                CellShapeType shape,
                                                const vtkm::exec::FunctorBase&)
{
  UNUSED(numPts);
  UNUSED(pts);
  UNUSED(avgArea);
  UNUSED(shape);
  return OutType(-1.);
}

// ========================= 2D cells ==================================

template <typename OutType, typename PointCoordVecType>
VTKM_EXEC OutType CellRelativeSizeSquaredMetric(const vtkm::IdComponent& numPts,
                                                const PointCoordVecType& pts,
                                                const OutType& avgArea,
                                                vtkm::CellShapeTagTriangle tag,
                                                const vtkm::exec::FunctorBase& worklet)
{
  UNUSED(worklet);
  if (numPts != 3)
  {
    worklet.RaiseError("Edge ratio metric(triangle) requires 3 points.");
    return OutType(-1.);
  }
  // fix this
  OutType A = vtkm::exec::CellMeasure<OutType>(numPts, pts, tag, worklet);
  OutType R = A / avgArea;
  if (R == OutType(0.))
    return OutType(0.);
  OutType q = vtkm::Pow(vtkm::Min(R, OutType(1.) / R), OutType(2.));
  return OutType(q);
}

template <typename OutType, typename PointCoordVecType>
VTKM_EXEC OutType CellRelativeSizeSquaredMetric(const vtkm::IdComponent& numPts,
                                                const PointCoordVecType& pts,
                                                const OutType& avgArea,
                                                vtkm::CellShapeTagQuad tag,
                                                const vtkm::exec::FunctorBase& worklet)
{
  UNUSED(worklet);
  if (numPts != 4)
  {
    worklet.RaiseError("Edge ratio metric(quadrilateral) requires 4 points.");
    return OutType(-1.);
  }
  // fix this
  OutType A = vtkm::exec::CellMeasure<OutType>(numPts, pts, tag, worklet);
  OutType R = A / avgArea;
  if (R == OutType(0.))
    return OutType(0.);
  OutType q = vtkm::Pow(vtkm::Min(R, OutType(1.) / R), OutType(2.));
  return OutType(q);
}

// ========================= 3D cells ==================================

template <typename OutType, typename PointCoordVecType>
VTKM_EXEC OutType CellRelativeSizeSquaredMetric(const vtkm::IdComponent& numPts,
                                                const PointCoordVecType& pts,
                                                const OutType& avgVolume,
                                                vtkm::CellShapeTagTetra tag,
                                                const vtkm::exec::FunctorBase& worklet)
{
  UNUSED(worklet);
  if (numPts != 4)
  {
    worklet.RaiseError("Edge ratio metric(tetrahedral) requires 4 points.");
    return OutType(-1.);
  }
  // fix this
  OutType V = vtkm::exec::CellMeasure<OutType>(numPts, pts, tag, worklet);
  OutType R = V / avgVolume;
  if (R == OutType(0.))
    return OutType(0.);
  OutType q = vtkm::Pow(vtkm::Min(R, OutType(1.) / R), OutType(2.));
  return OutType(q);
}

template <typename OutType, typename PointCoordVecType>
VTKM_EXEC OutType CellRelativeSizeSquaredMetric(const vtkm::IdComponent& numPts,
                                                const PointCoordVecType& pts,
                                                const OutType& avgVolume,
                                                vtkm::CellShapeTagHexahedron tag,
                                                const vtkm::exec::FunctorBase& worklet)
{
  UNUSED(worklet);
  if (numPts != 8)
  {
    worklet.RaiseError("Edge ratio metric(hexahedral) requires 8 points.");
    return OutType(-1.);
  }
  // fix this
  //OutType alpha8 = vtkm::Det(A8);
  //OutType D = alpha8/(OutType(64.)*avgVolume);
  //if( D == OutTyp(0.))
  //return OutType(0.);
  //OutType q = vtkm::Pow(vtkm::Min(D, OutType(1.)/D),OutType(2.));
  //return OutType(q);
  return 0;
}

} // namespace cellmetrics
} // namespace worklet
} // namespace vtkm

#endif // vtk_m_exec_cellmetrics_CellRelativeSizeSquaredMetric.h
