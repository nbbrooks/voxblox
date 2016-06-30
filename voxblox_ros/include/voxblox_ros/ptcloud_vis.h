#ifndef VOXBLOX_ROS_PTCLOUD_VIS_H_
#define VOXBLOX_ROS_PTCLOUD_VIS_H_

#include <algorithm>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <eigen_conversions/eigen_msg.h>
#include <string>

#include <voxblox/core/common.h>
#include <voxblox/core/layer.h>
#include <voxblox/core/voxel.h>

#include "voxblox_ros/conversions.h"

// This file contains a set of functions to visualize layers as pointclouds
// (or marker arrays) based on a passed-in function. It also offers some
// specializations of functions as samples.

namespace voxblox {

// Shortcut the placeholders namespace, since otherwise it chooses the boost
// placeholders which are in the global namespace (thanks boost!).
namespace ph = std::placeholders;

/// This is a fancy alias to be able to template functions.
template <typename VoxelType>
using ShouldVisualizeVoxelColorFunctionType = std::function<bool(
    const VoxelType& voxel, const Point& coord, Color* color)>;

// For intensities values, such as distances, which are mapped to a color only
// by the subscriber.
template <typename VoxelType>
using ShouldVisualizeVoxelIntensityFunctionType = std::function<bool(
    const VoxelType& voxel, const Point& coord, double* intensity)>;

// For boolean checks -- either a voxel is visualized or it is not.
// This is used for occupancy bricks, for instance.
template <typename VoxelType>
using ShouldVisualizeVoxelFunctionType =
    std::function<bool(const VoxelType& voxel, const Point& coord)>;  // NOLINT;

// Template function to visualize a colored pointcloud.
template <typename VoxelType>
void createColorPointcloudFromLayer(
    const Layer<VoxelType>& layer,
    const ShouldVisualizeVoxelColorFunctionType<VoxelType>& vis_function,
    pcl::PointCloud<pcl::PointXYZRGB>* pointcloud) {
  CHECK_NOTNULL(pointcloud);
  pointcloud->clear();
  BlockIndexList blocks;
  layer.getAllAllocatedBlocks(&blocks);

  // Cache layer settings.
  size_t vps = layer.voxels_per_side();
  size_t num_voxels_per_block = vps * vps * vps;

  // Temp variables.
  Color color;
  // Iterate over all blocks.
  for (const BlockIndex& index : blocks) {
    // Iterate over all voxels in said blocks.
    const Block<VoxelType>& block = layer.getBlockByIndex(index);

    for (size_t linear_index = 0; linear_index < num_voxels_per_block;
         ++linear_index) {
      Point coord = block.computeCoordinatesFromLinearIndex(linear_index);
      if (vis_function(block.getVoxelByLinearIndex(linear_index), coord,
                       &color)) {
        pcl::PointXYZRGB point;
        point.x = coord.x();
        point.y = coord.y();
        point.z = coord.z();
        point.r = color.r;
        point.g = color.g;
        point.b = color.b;
        pointcloud->push_back(point);
      }
    }
  }
}

// Template function to visualize an intensity pointcloud.
template <typename VoxelType>
void createColorPointcloudFromLayer(
    const Layer<VoxelType>& layer,
    const ShouldVisualizeVoxelIntensityFunctionType<VoxelType>& vis_function,
    pcl::PointCloud<pcl::PointXYZI>* pointcloud) {
  CHECK_NOTNULL(pointcloud);
  pointcloud->clear();
  BlockIndexList blocks;
  layer.getAllAllocatedBlocks(&blocks);

  // Cache layer settings.
  size_t vps = layer.voxels_per_side();
  size_t num_voxels_per_block = vps * vps * vps;

  // Temp variables.
  double intensity = 0.0;
  // Iterate over all blocks.
  for (const BlockIndex& index : blocks) {
    // Iterate over all voxels in said blocks.
    const Block<VoxelType>& block = layer.getBlockByIndex(index);

    for (size_t linear_index = 0; linear_index < num_voxels_per_block;
         ++linear_index) {
      Point coord = block.computeCoordinatesFromLinearIndex(linear_index);
      if (vis_function(block.getVoxelByLinearIndex(linear_index), coord,
                       &intensity)) {
        pcl::PointXYZI point;
        point.x = coord.x();
        point.y = coord.y();
        point.z = coord.z();
        point.intensity = intensity;
        pointcloud->push_back(point);
      }
    }
  }
}

template <typename VoxelType>
void createOccupancyBlocksFromLayer(
    const Layer<VoxelType>& layer,
    const ShouldVisualizeVoxelFunctionType<VoxelType>& vis_function,
    const std::string& frame_id,
    visualization_msgs::MarkerArray* marker_array) {
  CHECK_NOTNULL(marker_array);
  // Cache layer settings.
  size_t vps = layer.voxels_per_side();
  size_t num_voxels_per_block = vps * vps * vps;
  FloatingPoint voxel_size = layer.voxel_size();

  visualization_msgs::Marker block_marker;
  block_marker.header.frame_id = frame_id;
  block_marker.ns = "occupied_voxels";
  block_marker.id = 0;
  block_marker.type = visualization_msgs::Marker::CUBE_LIST;
  block_marker.scale.x = block_marker.scale.y = block_marker.scale.z =
      voxel_size;
  block_marker.action = visualization_msgs::Marker::ADD;

  BlockIndexList blocks;
  layer.getAllAllocatedBlocks(&blocks);
  for (const BlockIndex& index : blocks) {
    // Iterate over all voxels in said blocks.
    const Block<VoxelType>& block = layer.getBlockByIndex(index);

    for (size_t linear_index = 0; linear_index < num_voxels_per_block;
         ++linear_index) {
      Point coord = block.computeCoordinatesFromLinearIndex(linear_index);
      if (vis_function(block.getVoxelByLinearIndex(linear_index), coord)) {
        geometry_msgs::Point cube_center;
        cube_center.x = coord.x();
        cube_center.y = coord.y();
        cube_center.z = coord.z();
        block_marker.points.push_back(cube_center);
        std_msgs::ColorRGBA color_msg;
        colorVoxbloxToMsg(rainbowColorMap((coord.z() - 5) * 10), &color_msg);
        block_marker.colors.push_back(color_msg);
      }
    }
  }
  marker_array->markers.push_back(block_marker);
}

// Short-hand functions for visualizing different types of voxels.
bool visualizeNearSurfaceTsdfVoxels(const TsdfVoxel& voxel, const Point& coord,
                                    double surface_distance, Color* color) {
  if (voxel.weight > 0 && std::abs(voxel.distance) < surface_distance) {
    *color = voxel.color;
    return true;
  }
  return false;
}

bool visualizeDistanceIntensityTsdfVoxels(const TsdfVoxel& voxel,
                                          const Point& coord,
                                          double* intensity) {
  if (voxel.weight > 1e-3) {
    *intensity = voxel.distance;
    return true;
  }
  return false;
}

bool visualizeDistanceIntensityEsdfVoxels(const EsdfVoxel& voxel,
                                          const Point& coord,
                                          double* intensity) {
  if (voxel.observed) {
    *intensity = voxel.distance;
    return true;
  }
  return false;
}

bool visualizeOccupiedTsdfVoxels(const TsdfVoxel& voxel, const Point& coord) {
  if (voxel.weight > 1e-3 && voxel.distance <= 0) {
    return true;
  }
  return false;
}

// All functions that can be used directly for TSDF voxels.
void createSurfacePointcloudFromTsdfLayer(
    const Layer<TsdfVoxel>& layer, double surface_distance,
    pcl::PointCloud<pcl::PointXYZRGB>* pointcloud) {
  createColorPointcloudFromLayer<TsdfVoxel>(
      layer, std::bind(&visualizeNearSurfaceTsdfVoxels, ph::_1, ph::_2,
                       surface_distance, ph::_3),
      pointcloud);
}

void createDistancePointcloudFromTsdfLayer(
    const Layer<TsdfVoxel>& layer,
    pcl::PointCloud<pcl::PointXYZI>* pointcloud) {
  createColorPointcloudFromLayer<TsdfVoxel>(
      layer, &visualizeDistanceIntensityTsdfVoxels, pointcloud);
}

void createDistancePointcloudFromEsdfLayer(
    const Layer<EsdfVoxel>& layer,
    pcl::PointCloud<pcl::PointXYZI>* pointcloud) {
  createColorPointcloudFromLayer<EsdfVoxel>(
      layer, &visualizeDistanceIntensityEsdfVoxels, pointcloud);
}

void createOccupancyBlocksFromTsdfLayer(
    const Layer<TsdfVoxel>& layer, const std::string& frame_id,
    visualization_msgs::MarkerArray* marker_array) {
  createOccupancyBlocksFromLayer<TsdfVoxel>(layer, &visualizeOccupiedTsdfVoxels,
                                            frame_id, marker_array);
}

}  // namespace voxblox

#endif  // VOXBLOX_ROS_PTCLOUD_VIS_H_
