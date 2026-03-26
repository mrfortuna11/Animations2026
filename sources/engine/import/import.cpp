#include "render/mesh.h"
#include <vector>
#include <3dmath.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "engine/api.h"
#include "glad/glad.h"

#include "import/model.h"

MeshPtr create_mesh(const aiMesh *mesh)
{
  std::vector<uint32_t> indices;
  std::vector<vec3> vertices;
  std::vector<vec3> normals;
  std::vector<vec2> uv;
  std::vector<uvec4> boneIndexes;
  std::vector<vec4> boneWeights;

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
      const aiBone *bone = mesh->mBones[i];
      // bonesMap[std::string(bone->mName.C_Str())] = i;

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
  return create_mesh(mesh->mName.C_Str(), indices, vertices, normals, uv, boneWeights, boneIndexes);
}

static void load_skeleton(SkeletonOffline &skeleton, const aiNode &node, int parent, int depth)
{
  int nodeIndex = skeleton.names.size();
  skeleton.names.push_back(node.mName.C_Str());

  // aiVector3D scaling;
  // aiQuaternion rotation;
  // aiVector3D position;
  // node.mTransformation.Decompose(scaling, rotation, position);

  glm::mat4 localTransform;// = glm::translate(glm::mat4(1.f), to_vec3(position)) * to_mat4(rotation) * glm::scale(glm::mat4(1.f), to_vec3(scaling));
  memcpy(&localTransform, &node.mTransformation, sizeof(localTransform));
  localTransform = glm::transpose(localTransform);
  skeleton.localTransform.push_back(localTransform);

  skeleton.parents.push_back(parent);
  skeleton.hierarchyDepth.push_back(depth);

  for (int i = 0; i < node.mNumChildren; i++)
    load_skeleton(skeleton, *node.mChildren[i], nodeIndex, depth + 1);
}

ModelAsset load_model(const char *path)
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

  const aiScene *scene = importer.GetScene();
  ModelAsset model;
  model.path = path;
  if (!scene)
  {
    engine::error("Filed to read model file \"%s\"", path);
    return model;
  }

  load_skeleton(model.skeleton, *scene->mRootNode, -1, 0);

  model.meshes.resize(scene->mNumMeshes);
  for (uint32_t i = 0; i < scene->mNumMeshes; i++)
  {
    model.meshes[i] = create_mesh(scene->mMeshes[i]);
  }

  engine::log("Model \"%s\" loaded", path);
  return model;
}