#pragma once
#include "render/mesh.h"
#include <vector>

struct SkeletonOffline
{
  std::vector<std::string> names;
  std::vector<mat4> localTransform;
  std::vector<int> parents;
  std::vector<int> hierarchyDepth; // only for ui
};

struct ModelAsset
{
  std::string path;
  std::vector<MeshPtr> meshes;
  SkeletonOffline skeleton;
};

ModelAsset load_model(const char *path);
