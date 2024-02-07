//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================
// Copyright (c) 2018, The Regents of the University of California, through
// Lawrence Berkeley National Laboratory (subject to receipt of any required approvals
// from the U.S. Dept. of Energy).  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this
//     list of conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National
//     Laboratory, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=============================================================================
//
//  This code is an extension of the algorithm presented in the paper:
//  Parallel Peak Pruning for Scalable SMP Contour Tree Computation.
//  Hamish Carr, Gunther Weber, Christopher Sewell, and James Ahrens.
//  Proceedings of the IEEE Symposium on Large Data Analysis and Visualization
//  (LDAV), October 2016, Baltimore, Maryland.
//
//  The PPP2 algorithm and software were jointly developed by
//  Hamish Carr (University of Leeds), Gunther H. Weber (LBNL), and
//  Oliver Ruebel (LBNL)
//==============================================================================

#ifndef vtk_m_worklet_ContourTreeUniformAugmented_h
#define vtk_m_worklet_ContourTreeUniformAugmented_h


#include <sstream>
#include <utility>

// VTKM includes
#include <vtkm/Math.h>
#include <vtkm/Types.h>
#include <vtkm/cont/ArrayHandle.h>
#include <vtkm/cont/ArrayHandleCounting.h>
#include <vtkm/cont/Field.h>
#include <vtkm/cont/Timer.h>
#include <vtkm/worklet/DispatcherMapField.h>
#include <vtkm/worklet/WorkletMapField.h>

// Contour tree worklet includes
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/ActiveGraph.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/ContourTree.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/ContourTreeMaker.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/DataSetMesh.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/MergeTree.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/MeshExtrema.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/Types.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/meshtypes/ContourTreeMesh.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/meshtypes/mesh_boundary/MeshBoundary2D.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/meshtypes/mesh_boundary/MeshBoundary3D.h>
#include <vtkm/filter/scalar_topology/worklet/contourtree_augmented/meshtypes/mesh_boundary/MeshBoundaryContourTreeMesh.h>

using vtkm::worklet::contourtree_augmented::NO_SUCH_ELEMENT;

namespace vtkm
{
namespace worklet
{

/// Compute the contour tree for 2d and 3d uniform grids and arbitrary topology graphs
class ContourTreeAugmented
{
public:
  /*!
  * Log level to be used for outputting timing information. Default is vtkm::cont::LogLevel::Perf
  * Use vtkm::cont::LogLevel::Off to disable outputing the results via vtkm logging here. The
  * results are saved in the TimingsLogString variable so we can use it to do our own logging
  */
  vtkm::cont::LogLevel TimingsLogLevel = vtkm::cont::LogLevel::Perf;

  /// Remember the results from our time-keeping so we can customize our logging
  std::string TimingsLogString;


  /*!
  * Run the contour tree to merge an existing set of contour trees
  *
  *  fieldArray   : Needed only as a pass-through value but not used in this case
  *  mesh : The ContourTreeMesh for which the contour tree should be computed
  *  contourTree  : The output contour tree to be computed (output)
  *  sortOrder    : The sort order for the mesh vertices (output)
  *  nIterations  : The number of iterations used to compute the contour tree (output)
  *  computeRegularStructure : 0=Off, 1=full augmentation with all vertices
  *                            2=boundary augmentation using meshBoundary
  *  meshBoundary : This parameter is generated by calling mesh.GetMeshBoundaryExecutionObject
  *                 For regular 2D/3D meshes this required no extra parameters, however, for a
  *                 ContourTreeMesh additional information about the block must be given. Rather
  *                 than generating the MeshBoundary descriptor here, we therefore, require it
  *                 as an input. The MeshBoundary is used to augment the contour tree with the
  *                 mesh boundary vertices. It is needed only if we want to augement by the
  *                 mesh boundary and computeRegularStructure is False (i.e., if we compute
  *                 the full regular strucuture this is not needed because all vertices
  *                 (including the boundary) will be addded to the tree anyways.
  */
  template <typename FieldType,
            typename StorageType,
            typename MeshType,
            typename MeshBoundaryMeshExecType>
  void Run(const vtkm::cont::ArrayHandle<FieldType, StorageType> fieldArray,
           MeshType& mesh,
           contourtree_augmented::ContourTree& contourTree,
           contourtree_augmented::IdArrayType& sortOrder,
           vtkm::Id& nIterations,
           unsigned int computeRegularStructure,
           const MeshBoundaryMeshExecType& meshBoundary)
  {
    RunContourTree(
      fieldArray, // Just a place-holder to fill the required field. Used when calling SortData on the contour tree which is a no-op
      contourTree,
      sortOrder,
      nIterations,
      mesh,
      computeRegularStructure,
      meshBoundary);
    return;
  }

  /*!
   * Run the contour tree analysis. This helper function is used to
   * allow one to run the contour tree in a consistent fashion independent
   * of whether the data is 2D, 3D, or 3D_MC. This function initalizes
   * the approbritate mesh class from the contourtree_augmented worklet
   * and constructs ths mesh boundary exectuion object to be used. It the
   * subsequently calls RunContourTree method to compute the actual contour tree.
   *
   *  fieldArray   : Needed only as a pass-through value but not used in this case
   *  mesh : The ContourTreeMesh for which the contour tree should be computed
   *  contourTree  : The output contour tree to be computed (output)
   *  sortOrder    : The sort order for the mesh vertices (output)
   *  nIterations  : The number of iterations used to compute the contour tree (output)
   *  nRows        : Number of rows (i.e, x values) in the input mesh
   *  nCols        : Number of columns (i.e, y values) in the input mesh
   *  nSlices      : Number of slicex (i.e, z values) in the input mesh. Default is 1
   *                 to avoid having to set the nSlices for 2D input meshes
   *  useMarchingCubes : Boolean indicating whether marching cubes (true) or freudenthal (false)
   *                     connectivity should be used. Valid only for 3D input data. Default is false.
   *  computeRegularStructure : 0=Off, 1=full augmentation with all vertices
   *                            2=boundary augmentation using meshBoundary.
   */
  template <typename FieldType, typename StorageType>
  void Run(const vtkm::cont::ArrayHandle<FieldType, StorageType> fieldArray,
           contourtree_augmented::ContourTree& contourTree,
           contourtree_augmented::IdArrayType& sortOrder,
           vtkm::Id& nIterations,
           const vtkm::Id3 meshSize,
           bool useMarchingCubes = false,
           unsigned int computeRegularStructure = 1)
  {
    std::cout << "{sc_tp/worklet/ContourTreeUniformAugmented.h : Running augmented CT ...\n";
    using namespace vtkm::worklet::contourtree_augmented;
    // 2D Contour Tree
    if (meshSize[2] == 1)
    {
      std::cout << "Creating 2D Mesh with dim: " << meshSize[0] << "x" << meshSize[1] << "\n";
      // Build the mesh and fill in the values
      // DataSetMeshTriangulation2DFreudenthal mesh(vtkm::Id2{ meshSize[0], meshSize[1] });
      std::cout << "Contour Tree Mesh TEST from file ... \n";
      //ContourTreeMesh mesh("ctinput.txt");
      // runtime error when reading CT from existing output file ...
      // ... (created by PPP-2.0 save function) ...
      //ContourTreeMesh<int> mesh("5x5.all.txt");
      // ... so trying the default constructor ...
      // ... which in the VTK-m version does nothing (no-op) ...
      // ... so ported values from PPP-2.0 to this one to try with a valid CT:
      //ContourTreeMesh<int> mesh;
      // 5x5 dataset values:
      // std::vector<int> nodes_sorted_values = {100,78,49,17,1,94,71,47,33,6,52,44,50,45,48,8,12,46,91,43,0,5,51,76,83};
      // std::vector<int> arcs =
      // Take this simple graph for example:
      //
      /* 4
          \
           \> 3 -> 1 <- 0
           /
          /
         2
        (Use this comment style to avoid warnings about multi-line comments triggered by '\' at
         the end of the line).
      */
      // make all nodes/sort/values the same for easier debugging:
      //std::vector<vtkm::Id> std_nodes_sorted = {0, 1, 2, 3, 4};
      //std::vector<vtkm::Id> std_nodes_sorted = {0, 1, 2, 3, 4, 5, 6, 7};
      // below is for 5x5:
//      std::vector<vtkm::Id> std_nodes_sorted = {24, 20, 14, 6, 1,
//                                                23, 18, 12, 7, 3,
//                                                17, 9,  15, 10, 13,
//                                                4,  5,  11, 22, 8,
//                                                0,  2,  16, 19, 21};

      std::vector<vtkm::Id> std_nodes_sorted = {0, 1, 2, 3, 4,
                                                5, 6, 7, 8, 9,
                                                10, 11, 12, 13, 14,
                                                15, 16, 17, 18, 19,
                                                20, 21, 22, 23, 24};

      // convert regular std::vector to an array that VTK likes ... picky
      vtkm::cont::ArrayHandle<vtkm::Id> nodes_sorted =
        vtkm::cont::make_ArrayHandle(std_nodes_sorted, vtkm::CopyFlag::Off);

      //IdArrayType nodes_sorted(0, 1, 2, 3, 4); doesnt work
      // take 2
      //std::vector<int> std_actual_values = {0, 1, 2, 3, 4};
      //std::vector<int> std_actual_values = {0, 1, 2, 3, 4, 5, 6, 7};
      // below for 5x5:
//      std::vector<int> std_actual_values = {24, 20, 14, 6, 1,
//                                            23, 18, 12, 7, 3,
//                                            17, 9,  15, 10, 13,
//                                            4,  5,  11, 22, 8,
//                                            0,  2,  16, 19, 21};
      std::vector<int> std_actual_values = {0, 1, 2, 3, 4,
                                            5, 6, 7, 8, 9,
                                            10, 11, 12, 13, 14,
                                            15, 16, 17, 18, 19,
                                            20, 21, 22, 23, 24};

//      std::vector<int> std_actual_values = {100, 78, 49, 17, 1,
//                                            94, 71, 47, 33, 6,
//                                            52, 44, 50, 45, 48,
//                                            8, 12, 46, 91, 43,
//                                            0, 5, 51, 76, 83};

      // convert regular std::vector to an array that VTK likes ... picky
      vtkm::cont::ArrayHandle<int> actual_values =
        vtkm::cont::make_ArrayHandle(std_actual_values, vtkm::CopyFlag::Off);

      // The contour tree algorithm stores this in an arcs array:
      //
      // idx:  0 1 2 3 4
      // arcs: 1 - 3 1 3 (- = NO_SUCH_ELEMENT, meaning no arc originating from this node)
      //std::vector<vtkm::Id> std_arcs_list = {1, NO_SUCH_ELEMENT,  3, 1, 3};
//      std::vector<vtkm::Id> std_arcs_list = {2, 2, 3, 4, 5, 7, 5, NO_SUCH_ELEMENT};
      // just part of my standard conversion routine at this point:
//      vtkm::cont::ArrayHandle<vtkm::Id> arcs_list =
//        vtkm::cont::make_ArrayHandle(std_arcs_list, vtkm::CopyFlag::Off);

      // after a lengthy process ... finally the last one:
      std::vector<vtkm::Id> std_global_inds = {0};
      vtkm::cont::ArrayHandle<vtkm::Id> global_inds =
        vtkm::cont::make_ArrayHandle(std_global_inds, vtkm::CopyFlag::Off);

      // uncomment if testing the ContourTreeMesh
//      ContourTreeMesh<int> mesh(nodes_sorted,
//                                arcs_list,
//                                nodes_sorted,
//                                actual_values,
//                                global_inds);

      // WARNING WARNING WARNING!
      // The following is to test the Topology Graph which is still in an unfinished state!
//      std::vector<vtkm::Id> std_nbor_connectivity = {2, 4,
//                                                     2, 3,
//                                                     0, 1, 3, 4,
//                                                     1, 2, 4, 5, 7,
//                                                     0, 2, 3, 5, 6,
//                                                     3, 4, 6, 7,
//                                                     4, 5,
//                                                     3, 5};
      // below for 5x5:
//      std::vector<vtkm::Id> std_nbor_connectivity = {18, 20, 23,                // [0] 24
//                                                     12, 14, 18, 24,            // [1] 20
//                                                     6,  7,  12, 20,            // [2] 14
//                                                     1,  3,  7,  14,            // [3] 6
//                                                     3,  6,                     // [4] 1
//                                                     9,  17, 18, 24,            // [5] 23
//                                                     9,  12, 15, 20, 23, 24,    // [6] 18
//                                                     7,  10, 14, 15, 18, 20,    // [7] 12
//                                                     3,   6, 10, 12, 13, 14,    // [8] 7
//                                                     1,  6,  7,  13,            // [9] 3
//                                                     4,  5,  9,  23,            // [10] 17
//                                                     5, 11, 15, 17, 18, 23,     // [11] 9
//                                                     9, 10, 11, 12, 18, 22,     // [12] 15
//                                                     7, 8, 12, 13, 15, 22,      // [13] 10
//                                                     3, 7, 8, 10,               // [14] 13
//                                                     0, 2, 5, 17,               // [15] 4
//                                                     2, 4, 9, 11, 16, 17,       // [16] 5
//                                                     5, 9, 15, 16, 19, 22,      // [17] 11
//                                                     8, 10, 11, 15, 19, 21,     // [18] 22
//                                                     10, 13, 21, 22,            // [19] 8
//                                                     2, 4,                      // [20] 0
//                                                     0, 4, 5, 16,               // [21] 2
//                                                     2, 5, 11, 19,              // [22] 16
//                                                     11, 16, 21, 22,            // [23] 19
//                                                     8, 19, 22};                // [24] 21

      std::vector<vtkm::Id> std_nbor_connectivity = {2,4,
                                                     3,6,
                                                     0,4,5,16,
                                                     1,6,7,13,
                                                     0,2,5,17,
                                                     2,4,9,11,16,17,
                                                     1,3,7,14,
                                                     3,6,10,12,13,14,
                                                     10,13,21,22,
                                                     5,11,15,17,18,23,
                                                     7,8,12,13,15,22,
                                                     5,9,15,16,19,22,
                                                     7,10,14,15,18,20,
                                                     3,7,8,10,
                                                     6,7,12,20,
                                                     9,10,11,12,18,22,
                                                     2,5,11,19,
                                                     4,5,9,23,
                                                     9,12,15,20,23,24,
                                                     11,16,21,22,
                                                     12,14,18,24,
                                                     8,19,22,
                                                     8,10,11,15,19,21,
                                                     9,17,18,24,
                                                     18,20,23,};                // [24] 21


//      std::vector<vtkm::Id> std_nbor_connectivity = {1,5,6,
//                                                     0,2,6,7,
//                                                     1,3,7,8,
//                                                     2,4,8,9,
//                                                     3,9,
//                                                     0,6,10,11,
//                                                     0,1,5,7,11,12,
//                                                     1,2,6,8,12,13,
//                                                     2,3,7,9,13,14,
//                                                     3,4,8,14,
//                                                     5,11,15,16,
//                                                     5,6,10,12,16,17,
//                                                     6,7,11,13,17,18,
//                                                     7,8,12,14,18,19,
//                                                     8,9,13,19,
//                                                     10,16,20,21,
//                                                     10,11,15,17,21,22,
//                                                     11,12,16,18,22,23,
//                                                     12,13,17,19,23,24,
//                                                     13,14,18,24,
//                                                     15,21,
//                                                     15,16,20,22,
//                                                     16,17,21,23,
//                                                     17,18,22,24,
//                                                     18,19,23,};                // [24] 21



      vtkm::cont::ArrayHandle<vtkm::Id> nbor_connectivity =
        vtkm::cont::make_ArrayHandle(std_nbor_connectivity, vtkm::CopyFlag::Off);

      //std::vector<vtkm::Id> std_nbor_offsets = {0, 2, 4, 8, 13, 18, 22, 24, 26};
      // below for 5x5:
    //std::vector<vtkm::Id> std_nbor_offsets = {0, 3, 7, 11, 15, 17, 21, 27, 33, 39, 43, 47, 53, 59, 65, 69, 73, 79, 85, 91, 95, 97, 101, 105, 109, 112};
      std::vector<vtkm::Id> std_nbor_offsets = {0,2,4,8,12,16,22,26,32,36,42,48,54,60,64,68,74,78,82,88,92,96,99,105,109,112};
      vtkm::cont::ArrayHandle<vtkm::Id> nbor_offsets =
        vtkm::cont::make_ArrayHandle(std_nbor_offsets, vtkm::CopyFlag::Off);

      ContourTreeMesh<int> mesh(nodes_sorted,
                              //arcs_list,
                                nbor_connectivity,
                                nbor_offsets,
                                nodes_sorted,
                                actual_values,
                                global_inds);

      //ContourTreeMesh<int> mesh("5x5.all.txt");
      std::cout << "Finished reading, printing content:\n";
      mesh.PrintContent(std::cout);
      std::cout << "Finished printing content\n";

      // Run the contour tree on the mesh
      RunContourTree(fieldArray,
                     contourTree,
                     sortOrder,
                     nIterations,
                     mesh,
                     computeRegularStructure,
                     //nullptr);
                     mesh.GetMeshBoundaryExecutionObject());
      std::cout << "sc_tp/worklet/ContourTreeUniformAugmented.h : Change to NULL ptr here}\n";
      return;
    }
    // 3D Contour Tree using marching cubes
    else if (useMarchingCubes)
    {
      // Build the mesh and fill in the values
      DataSetMeshTriangulation3DMarchingCubes mesh(meshSize);
      // Run the contour tree on the mesh
      RunContourTree(fieldArray,
                     contourTree,
                     sortOrder,
                     nIterations,
                     mesh,
                     computeRegularStructure,
                     mesh.GetMeshBoundaryExecutionObject());
      return;
    }
    // 3D Contour Tree with Freudenthal
    else
    {
      // Build the mesh and fill in the values
      DataSetMeshTriangulation3DFreudenthal mesh(meshSize);
      // Run the contour tree on the mesh
      RunContourTree(fieldArray,
                     contourTree,
                     sortOrder,
                     nIterations,
                     mesh,
                     computeRegularStructure,
                     mesh.GetMeshBoundaryExecutionObject());
      return;
    }
  }


private:
  /*!
  *  Run the contour tree for the given mesh. This function implements the main steps for
  *  computing the contour tree after the mesh has been constructed using the approbrite
  *  contour tree mesh class.
  *
  *  fieldArray   : The values of the mesh
  *  contourTree  : The output contour tree to be computed (output)
  *  sortOrder    : The sort order for the mesh vertices (output)
  *  nIterations  : The number of iterations used to compute the contour tree (output)
  *  mesh : The specific mesh (see vtkm/worklet/contourtree_augmented/mesh_dem_meshtypes
  *  computeRegularStructure : 0=Off, 1=full augmentation with all vertices
  *                            2=boundary augmentation using meshBoundary
  *  meshBoundary : This parameter is generated by calling mesh.GetMeshBoundaryExecutionObject
  *                 For regular 2D/3D meshes this required no extra parameters, however, for a
  *                 ContourTreeMesh additional information about the block must be given. Rather
  *                 than generating the MeshBoundary descriptor here, we therefore, require it
  *                 as an input. The MeshBoundary is used to augment the contour tree with the
  *                 mesh boundary vertices. It is needed only if we want to augement by the
  *                 mesh boundary and computeRegularStructure is False (i.e., if we compute
  *                 the full regular strucuture this is not needed because all vertices
  *                 (including the boundary) will be addded to the tree anyways.
  */
  template <typename FieldType,
            typename StorageType,
            typename MeshClass,
            typename MeshBoundaryClass>
  void RunContourTree(const vtkm::cont::ArrayHandle<FieldType, StorageType> fieldArray,
                      contourtree_augmented::ContourTree& contourTree,
                      contourtree_augmented::IdArrayType& sortOrder,
                      vtkm::Id& nIterations,
                      MeshClass& mesh,
                      unsigned int computeRegularStructure,
                      const MeshBoundaryClass& meshBoundary)
  {
    using namespace vtkm::worklet::contourtree_augmented;
    // Stage 1: Load the data into the mesh. This is done in the Run() method above and accessible
    //          here via the mesh parameter. The actual data load is performed outside of the
    //          worklet in the example contour tree app (or whoever uses the worklet)
    std::cout << "S1. {worklet/ContourTreeUniformAugmented.h : RunContourTree}\n";
    // Stage 2 : Sort the data on the mesh to initialize sortIndex & indexReverse on the mesh
    // Start the timer for the mesh sort
    std::cout << "S2\n";
    vtkm::cont::Timer timer;
    timer.Start();
    std::stringstream timingsStream; // Use a string stream to log in one message

    // Sort the mesh data
    mesh.SortData(fieldArray);
    timingsStream << "    " << std::setw(38) << std::left << "Sort Data"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
    timer.Start();

    // Stage 3: Assign every mesh vertex to a peak
    std::cout << "S3\n";
    MeshExtrema extrema(mesh.NumVertices);
    extrema.SetStarts(mesh, true);
    extrema.BuildRegularChains(true);
    timingsStream << "    " << std::setw(38) << std::left << "Join Tree Regular Chains"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
    timer.Start();

    // Stage 4: Identify join saddles & construct Active Join Graph
    std::cout << "S4\n";
    MergeTree joinTree(mesh.NumVertices, true);
    ActiveGraph joinGraph(true);
    joinGraph.Initialise(mesh, extrema);
    timingsStream << "    " << std::setw(38) << std::left << "Join Tree Initialize Active Graph"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;

#ifdef DEBUG_PRINT
    joinGraph.DebugPrint("Active Graph Instantiated", __FILE__, __LINE__);
#endif
    timer.Start();

    // Stage 5: Compute Join Tree Hyperarcs from Active Join Graph
    std::cout << "S5\n";
    joinGraph.MakeMergeTree(joinTree, extrema);
    timingsStream << "    " << std::setw(38) << std::left << "Join Tree Compute"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
#ifdef DEBUG_PRINT
    joinTree.DebugPrint("Join tree Computed", __FILE__, __LINE__);
    joinTree.DebugPrintTree("Join tree", __FILE__, __LINE__, mesh);
#endif
    timer.Start();

    // Stage 6: Assign every mesh vertex to a pit
    std::cout << "S6\n";
    extrema.SetStarts(mesh, false);
    extrema.BuildRegularChains(false);
    timingsStream << "    " << std::setw(38) << std::left << "Split Tree Regular Chains"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
    timer.Start();

    // Stage 7:     Identify split saddles & construct Active Split Graph
    std::cout << "S7\n";
    MergeTree splitTree(mesh.NumVertices, false);
    ActiveGraph splitGraph(false);
    splitGraph.Initialise(mesh, extrema);
    timingsStream << "    " << std::setw(38) << std::left << "Split Tree Initialize Active Graph"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
#ifdef DEBUG_PRINT
    splitGraph.DebugPrint("Active Graph Instantiated", __FILE__, __LINE__);
#endif
    timer.Start();

    // Stage 8: Compute Split Tree Hyperarcs from Active Split Graph
    std::cout << "S8\n";
    splitGraph.MakeMergeTree(splitTree, extrema);
    timingsStream << "    " << std::setw(38) << std::left << "Split Tree Compute"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
#ifdef DEBUG_PRINT
    splitTree.DebugPrint("Split tree Computed", __FILE__, __LINE__);
    // Debug split and join tree
    joinTree.DebugPrintTree("Join tree", __FILE__, __LINE__, mesh);
    splitTree.DebugPrintTree("Split tree", __FILE__, __LINE__, mesh);
#endif
    timer.Start();

    // Stage 9: Join & Split Tree are Augmented, then combined to construct Contour Tree
    std::cout << "S9\n";
    contourTree.Init(mesh.NumVertices);
    ContourTreeMaker treeMaker(contourTree, joinTree, splitTree);
    // 9.1 First we compute the hyper- and super- structure
    treeMaker.ComputeHyperAndSuperStructure();
    timingsStream << "    " << std::setw(38) << std::left
                  << "Contour Tree Hyper and Super Structure"
                  << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
    timer.Start();

    // 9.2 Then we compute the regular structure
    if (computeRegularStructure == 1) // augment with all vertices
    {
      treeMaker.ComputeRegularStructure(extrema);
      timingsStream << "    " << std::setw(38) << std::left << "Contour Tree Regular Structure"
                    << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
    }
    else if (computeRegularStructure == 2) // augment by the mesh boundary
    {
	  std::cout << "computeRegularStructure with dummy meshBoundary ...\n";
      treeMaker.ComputeBoundaryRegularStructure(extrema, mesh, meshBoundary);
      timingsStream << "    " << std::setw(38) << std::left
                    << "Contour Tree Boundary Regular Structure"
                    << ": " << timer.GetElapsedTime() << " seconds" << std::endl;
    }
    timer.Start();

    // Collect the output data
    nIterations = treeMaker.ContourTreeResult.NumIterations;
    //  Need to make a copy of sortOrder since ContourTreeMesh uses a smart array handle
    // TODO: Check if we can just make sortOrder a return array with variable type or if we can make the SortOrder return optional
    // TODO/FIXME: According to Ken Moreland the short answer is no. We may need to go back and refactor this when we
    // improve the contour tree API. https://gitlab.kitware.com/vtk/vtk-m/-/merge_requests/2263#note_831128 for more details.
    vtkm::cont::Algorithm::Copy(mesh.SortOrder, sortOrder);
    // ProcessContourTree::CollectSortedSuperarcs<DeviceAdapter>(contourTree, mesh.SortOrder, saddlePeak);
    // contourTree.SortedArcPrint(mesh.SortOrder);
    // contourTree.PrintDotSuperStructure();

    // Log the collected timing results in one coherent log entry
    this->TimingsLogString = timingsStream.str();
    if (this->TimingsLogLevel != vtkm::cont::LogLevel::Off)
    {
      VTKM_LOG_S(this->TimingsLogLevel,
                 std::endl
                   << "    ------------------- Contour Tree Worklet Timings ----------------------"
                   << std::endl
                   << this->TimingsLogString);
    }
  }
};

} // namespace vtkm
} // namespace vtkm::worklet

#endif // vtk_m_worklet_ContourTreeUniformAugmented_h
