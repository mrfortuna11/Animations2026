
#include "scene.h"

void render_character(const Character& character, const mat4& cameraProjView, vec3 cameraPosition, const DirectionLight& light)
{
  const Material& material = *character.material;
  const Shader& shader = material.get_shader();

  shader.use();
  material.bind_uniforms_to_shader();
  // shader.set_mat4x4("Transform", character.transform); // already opmized
  shader.set_mat4x4("ViewProjection", cameraProjView);
  shader.set_vec3("CameraPosition", cameraPosition);
  shader.set_vec3("LightDirection", glm::normalize(light.lightDirection));
  shader.set_vec3("AmbientLight", light.ambient);
  shader.set_vec3("SunLight", light.lightColor);

  std::span<const mat4> bindPose = {
    (const glm::mat4*)character.animationContext.worldTransforms.data(),
    character.animationContext.worldTransforms.size()
  };
  std::vector<mat4> skinningMatrixes;
  skinningMatrixes.reserve(bindPose.size());


  for (const MeshPtr& mesh : character.meshes)
  {
    skinningMatrixes.resize(mesh->inverseBindPose.size());
    for (size_t i = 0; i < mesh->inverseBindPose.size(); i++)
    {
      auto it = character.skeletonData.nodesMap.find(mesh->bonesNames[i]);
      if (it == character.skeletonData.nodesMap.end())
      {
        const std::string& name = mesh->bonesNames[i];
        engine::error("Bone \"%s\" from Mesh \"%s\" not found in skeleton", mesh->name.c_str(), name.c_str());
        skinningMatrixes[i] = glm::identity<glm::mat4>();
      }
      else
      {
        int nodeInSkeletonIdx = it->second;
        skinningMatrixes[i] = bindPose[nodeInSkeletonIdx] * mesh->inverseBindPose[i];
      }
    }
    shader.set_mat4x4("SkinningMatrixes", skinningMatrixes.data(), skinningMatrixes.size());
    render(mesh);
  }
}

void application_render(Scene& scene)
{
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  const float grayColor = 0.3f;
  glClearColor(grayColor, grayColor, grayColor, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


  const mat4& projection = scene.userCamera.projection;
  const glm::mat4& transform = scene.userCamera.transform;
  mat4 projView = projection * inverse(transform);

  for (const Character& character : scene.characters)
    render_character(character, projView, glm::vec3(transform[3]), scene.light);
}
