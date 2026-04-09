#pragma once
#include "engine/3dmath.h"
#include "engine/render/material.h"
#include "engine/render/mesh.h"
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/maths/soa_transform.h>

struct SkeletonData
{
  std::vector<std::string> names;
  std::vector<int> hierarchyDepth;  // only for ui
  std::vector<int> parents;
  std::map<std::string, int> nodesMap;

  SkeletonData() = default;

  explicit SkeletonData(const SkeletonOffline& skeleton)
    : names(skeleton.names),
    hierarchyDepth(skeleton.hierarchyDepth),
    parents(skeleton.parents)
  {
    for (size_t i = 0; i < names.size(); ++i) {
      nodesMap.emplace(names[i], static_cast<int>(i));
    }
  }
};

struct AnimationContext
{
  std::vector<ozz::math::SoaTransform> localTransforms;
  std::vector<ozz::math::Float4x4> worldTransforms;
  const ozz::animation::Skeleton* skeleton{ nullptr };
  const ozz::animation::Animation* currentAnimation{ nullptr };
  std::unique_ptr<ozz::animation::SamplingJob::Context> samplingCache;
  float currentProgress{ 0.0f };

  void setup(const ozz::animation::Skeleton* _skeleton)
  {
    if (!_skeleton) {
      return; 
    }

    skeleton = _skeleton;
    const size_t numSoaJoints = skeleton->num_soa_joints();
    const size_t numJoints = skeleton->num_joints();

    localTransforms.resize(numSoaJoints);
    worldTransforms.resize(numJoints);

    if (!samplingCache) {
      samplingCache = std::make_unique<ozz::animation::SamplingJob::Context>(numJoints);
    }
    else {
      samplingCache->Resize(numJoints);
    }
  }
};

struct AnimationEntry
{
  std::string name;
  const ozz::animation::Animation* animation;
};

struct Character
{
  std::string name;
  glm::mat4 transform;
  std::vector<MeshPtr> meshes;
  MaterialPtr material;
  SkeletonData skeletonData;
  AnimationContext animationContext;

  std::vector<AnimationEntry> availableAnimations;
  int currentAnimationIndex = 0;
};