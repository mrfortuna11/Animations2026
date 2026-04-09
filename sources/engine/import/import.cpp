#include "render/mesh.h"
#include <vector>
#include <3dmath.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "engine/api.h"
#include "glad/glad.h"

#include "import/model.h"

MeshPtr create_mesh(const aiMesh* mesh)
{
  std::vector<uint32_t> indices;
  std::vector<vec3> vertices;
  std::vector<vec3> normals;
  std::vector<vec2> uv;
  std::vector<uvec4> boneIndexes;
  std::vector<vec4> boneWeights;
  std::vector<mat4> inverseBindPose;
  std::vector<std::string> bonesNames;
  std::map<std::string, int> bonesMap;

  int numVert = mesh->mNumVertices;
  int numFaces = mesh->mNumFaces;

  if (mesh->HasFaces())
  {
    indices.resize(numFaces * 3);
    for (int i = 0; i < numFaces; i++)
    {
      assert(mesh->mFaces[i].mNumIndices == 3);
      for (int j = 0; j < 3; j++)
        indices[i * 3 + j] = mesh->mFaces[i].mIndices[j];
    }
  }

  if (mesh->HasPositions())
  {
    vertices.resize(numVert);
    for (int i = 0; i < numVert; i++)
      vertices[i] = to_vec3(mesh->mVertices[i]);
  }

  if (mesh->HasNormals())
  {
    normals.resize(numVert);
    for (int i = 0; i < numVert; i++)
      normals[i] = to_vec3(mesh->mNormals[i]);
  }

  if (mesh->HasTextureCoords(0))
  {
    uv.resize(numVert);
    for (int i = 0; i < numVert; i++)
      uv[i] = to_vec2(mesh->mTextureCoords[0][i]);
  }

  if (mesh->HasBones())
  {
    boneWeights.resize(numVert, vec4(0.f));
    boneIndexes.resize(numVert);

    int numBones = mesh->mNumBones;
    std::vector<int> weightsOffset(numVert, 0);
    for (int i = 0; i < numBones; i++)
    {
      const aiBone* bone = mesh->mBones[i];
      glm::mat4 mOffsetMatrix;
      memcpy(&mOffsetMatrix, &bone->mOffsetMatrix, sizeof(mOffsetMatrix));
      mOffsetMatrix = glm::transpose(mOffsetMatrix);
      inverseBindPose.push_back(mOffsetMatrix);
      bonesNames.push_back(bone->mName.C_Str());
      bonesMap[std::string(bone->mName.C_Str())] = i;

      for (unsigned j = 0; j < bone->mNumWeights; j++)
      {
        int vertex = bone->mWeights[j].mVertexId;
        int offset = weightsOffset[vertex]++;
        assert(offset < 4);
        boneWeights[vertex][offset] = bone->mWeights[j].mWeight;
        boneIndexes[vertex][offset] = i;
      }
    }
    // the sum of weights not 1
    for (int i = 0; i < numVert; i++)
    {
      vec4 w = boneWeights[i];
      float s = w.x + w.y + w.z + w.w;
      boneWeights[i] *= 1.f / s;
    }
  }
  return create_mesh(mesh->mName.C_Str(), indices, vertices, normals, uv, boneWeights, boneIndexes, std::move(inverseBindPose), std::move(bonesNames), std::move(bonesMap));
}

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>

using RawSkeleton = ozz::animation::offline::RawSkeleton;
using Joint = ozz::animation::offline::RawSkeleton::Joint;

static void load_skeleton(Joint& joint, SkeletonOffline& skeleton, const aiNode& node, int parent, int depth)
{
  int nodeIndex = skeleton.names.size();
  skeleton.names.push_back(node.mName.C_Str());
  // Setup joints name.
  joint.name = node.mName.C_Str();

  aiVector3D scaling;
  aiQuaternion rotation;
  aiVector3D position;
  node.mTransformation.Decompose(scaling, rotation, position);

  // Setup root joints bind-pose/rest transformation, in joint local-space.
  // This is the default skeleton posture (most of the time a T-pose). It's
  // used as a fallback when there's no animation for a joint.
  joint.transform.translation = ozz::math::Float3(position.x, position.y, position.z);
  joint.transform.rotation = ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
  joint.transform.scale = ozz::math::Float3(scaling.x, scaling.y, scaling.z);

  glm::mat4 localTransform;// = glm::translate(glm::mat4(1.f), to_vec3(position)) * to_mat4(rotation) * glm::scale(glm::mat4(1.f), to_vec3(scaling));
  memcpy(&localTransform, &node.mTransformation, sizeof(localTransform));
  localTransform = glm::transpose(localTransform);
  skeleton.localTransform.push_back(localTransform);

  skeleton.parents.push_back(parent);
  skeleton.hierarchyDepth.push_back(depth);

  // Now adds children to the joint.
  joint.children.resize(node.mNumChildren);
  for (int i = 0; i < node.mNumChildren; i++)
  {
    load_skeleton(joint.children[i], skeleton, *node.mChildren[i], nodeIndex, depth + 1);
  }
}

AnimationPtr create_animation(const aiAnimation* animation, const SkeletonPtr& skeleton)
{
  // Creates a RawAnimation.
  ozz::animation::offline::RawAnimation raw_animation;

  raw_animation.name = animation->mName.C_Str();
  // Sets animation duration.
  // All the animation keyframes times must be within range [0, duration].
  raw_animation.duration = animation->mDuration / animation->mTicksPerSecond; // ticks to seconds

  // Creates animation tracks.
  // There should be as much tracks as there are joints in the skeleton that
  // this animation targets.

  raw_animation.tracks.resize(skeleton->num_joints());
  for (int k = 0; k < skeleton->num_joints(); k++)
  {
    std::string_view jointName = skeleton->joint_names()[k];
    int channelIdx = -1;
    for (int i = 0; i < animation->mNumChannels; i++)
    {
      const aiNodeAnim& channel = *animation->mChannels[i];
      if (jointName == channel.mNodeName.C_Str())
      {
        channelIdx = i;
        break;
      }
    }
    ozz::animation::offline::RawAnimation::JointTrack& track = raw_animation.tracks[k];
    if (channelIdx == -1)
    {
      ozz::math::Transform transform = GetJointLocalRestPose(*skeleton, k);
      track.translations.resize(1);
      track.translations[0].time = 0.f;
      track.translations[0].value = transform.translation;

      track.rotations.resize(1);
      track.rotations[0].time = 0.f;
      track.rotations[0].value = transform.rotation;

      track.scales.resize(1);
      track.scales[0].time = 0.f;
      track.scales[0].value = transform.scale;
    }
    else
    {
      const aiNodeAnim& channel = *animation->mChannels[channelIdx];
      // Move datas from Assimp to ozz. Channal to track

      track.translations.resize(channel.mNumPositionKeys);
      for (int j = 0; j < channel.mNumPositionKeys; j++)
      {
        const aiVectorKey& key = channel.mPositionKeys[j];
        const float mTime = key.mTime / animation->mTicksPerSecond;
        track.translations[j].time = mTime;
        track.translations[j].value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
        assert(mTime >= 0.f && mTime <= raw_animation.duration);
        if (j > 0)
          assert(key.mTime >= channel.mPositionKeys[j - 1].mTime);
      }

      track.rotations.resize(channel.mNumRotationKeys);
      for (int j = 0; j < channel.mNumRotationKeys; j++)
      {
        const aiQuatKey& key = channel.mRotationKeys[j];
        const float mTime = key.mTime / animation->mTicksPerSecond;
        track.rotations[j].time = mTime;
        track.rotations[j].value = ozz::math::Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
        assert(mTime >= 0.f && mTime <= raw_animation.duration);
        if (j > 0)
          assert(key.mTime >= channel.mRotationKeys[j - 1].mTime);
      }

      track.scales.resize(channel.mNumScalingKeys);
      for (int j = 0; j < channel.mNumScalingKeys; j++)
      {
        const aiVectorKey& key = channel.mScalingKeys[j];
        const float mTime = key.mTime / animation->mTicksPerSecond;
        track.scales[j].time = mTime;
        track.scales[j].value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
        assert(mTime >= 0.f && mTime <= raw_animation.duration);
        if (j > 0)
          assert(key.mTime >= channel.mScalingKeys[j - 1].mTime);
      }
    }
  }

  // Test for animation validity. These are the errors that could invalidate
  // an animation:
  //  1. Animation duration is less than 0.
  //  2. Keyframes' are not sorted in a strict ascending order.
  //  3. Keyframes' are not within [0, duration] range.
  if (!raw_animation.Validate()) {
    // Instead of asserting (which may be stripped in release builds),
    // return a null/empty pointer to indicate failure
    engine::log("Animation validation failed");
    return {};
  }

  // Creates an AnimationBuilder instance.
  ozz::animation::offline::AnimationBuilder builder;

  AnimationPtr animationPtr = builder(raw_animation);

  if (animationPtr) {
    engine::log("Animation \"%s\" loaded", animation->mName.C_Str());
  }
  else {
    engine::log("Failed to build animation \"%s\"", animation->mName.C_Str());
  }

  return animationPtr;
}

ModelAsset load_model(const char* path)
{

  Assimp::Importer importer;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
  importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.f);

  importer.ReadFile(path,
    aiPostProcessSteps::aiProcess_Triangulate |
    aiPostProcessSteps::aiProcess_LimitBoneWeights |
    aiPostProcessSteps::aiProcess_GenNormals |
    aiPostProcessSteps::aiProcess_GlobalScale |
    aiPostProcessSteps::aiProcess_FlipWindingOrder);

  const aiScene* scene = importer.GetScene();
  ModelAsset model;
  model.path = path;
  if (!scene)
  {
    engine::error("Filed to read model file \"%s\"", path);
    return model;
  }
  RawSkeleton raw_skeleton;
  // Creates the root joint. Uses std::vector API to resize the number of roots.
  raw_skeleton.roots.resize(1);
  load_skeleton(raw_skeleton.roots[0], model.skeleton, *scene->mRootNode, -1, 0);

  // Test for skeleton validity.
  // The main invalidity reason is the number of joints, which must be lower
  // than ozz::animation::Skeleton::kMaxJoints.
  if (!raw_skeleton.Validate())
  {
    assert(false);
  }
  // Creates a SkeletonBuilder instance.
  ozz::animation::offline::SkeletonBuilder builder;

  SkeletonPtr skeleton = builder(raw_skeleton);
  model.skeleton.skeleton_ptr = std::move(skeleton);

  model.meshes.resize(scene->mNumMeshes);
  for (uint32_t i = 0; i < scene->mNumMeshes; i++)
  {
    model.meshes[i] = create_mesh(scene->mMeshes[i]);
  }

  model.animations.resize(scene->mNumAnimations);
  for (uint32_t i = 0; i < scene->mNumAnimations; i++)
  {
    model.animations[i] = create_animation(scene->mAnimations[i], model.skeleton.skeleton_ptr);
  }

  engine::log("Model \"%s\" loaded", path);
  return model;
}