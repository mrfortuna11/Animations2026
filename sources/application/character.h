#pragma once
#include "engine/3dmath.h"
#include "engine/render/material.h"
#include "engine/render/mesh.h"

struct SkeletonRuntime
{
  std::vector<std::string> names;
  std::vector<int> hierarchyDepth; // only for ui
  std::vector<int> parents;

  std::vector<mat4> localTransform;
  std::vector<mat4> worldTransform;
  SkeletonRuntime(const SkeletonOffline &skeleton) : names(skeleton.names), hierarchyDepth(skeleton.hierarchyDepth), parents(skeleton.parents)
  {
    localTransform = skeleton.localTransform;
    worldTransform.resize(localTransform.size(), mat4(1.f));
    forward_kinematics(skeleton);
  }

  void forward_kinematics(const SkeletonOffline &skeleton)
  {
    for (size_t i = 0; i < skeleton.parents.size(); i++)
    {
      int parent = skeleton.parents[i];
      if (parent != -1)
        worldTransform[i] = worldTransform[parent] * localTransform[i];
      else
        worldTransform[i] = localTransform[i];
    }
  }
};

struct Character
{
  std::string name;
  glm::mat4 transform;
  std::vector<MeshPtr> meshes;
  MaterialPtr material;
  SkeletonRuntime skeleton;
};
