#include "imgui/imgui.h"
#include "imgui/ImGuizmo.h"

#include "scene.h"

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

static void show_info()
{
  if (ImGui::Begin("Info"))
  {
    ImGui::Text("ESC - exit");
    ImGui::Text("F5 - recompile shaders");
    ImGui::Text("Left Mouse Button and Wheel - controll camera");
  }
  ImGui::End();
}

static glm::vec2 world_to_screen(const UserCamera &camera, glm::vec3 world_position)
{
  glm::vec4 clipSpace = camera.projection * inverse(camera.transform) * glm::vec4(world_position, 1.f);
  glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
  glm::vec2 screen = (glm::vec2(ndc) + 1.f) / 2.f;
  ImGuiIO &io = ImGui::GetIO();
  return glm::vec2(screen.x * io.DisplaySize.x, io.DisplaySize.y - screen.y * io.DisplaySize.y);
}

static void manipulate_transform(glm::mat4 &transform, const UserCamera &camera)
{
  ImGuizmo::BeginFrame();
  const glm::mat4 &projection = camera.projection;
  mat4 cameraView = inverse(camera.transform);
  ImGuiIO &io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  glm::mat4 globNodeTm = transform;

  ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(projection), mCurrentGizmoOperation, mCurrentGizmoMode,
                       glm::value_ptr(globNodeTm));

  transform = globNodeTm;
}

static void show_characters(Scene &scene)
{
  // implement showing characters when only one character can be selected
  static uint32_t selectedCharacter = -1u;
  static uint32_t selectedNode = -1u;
  if (ImGui::Begin("Scene"))
  {
    for (size_t i = 0; i < scene.characters.size(); i++)
    {
      Character &character = scene.characters[i];
      ImGui::PushID(i);
      if (ImGui::Selectable(character.name.c_str(), selectedCharacter == i, ImGuiSelectableFlags_AllowDoubleClick))
      {
        selectedCharacter = i;
        if (ImGui::IsMouseDoubleClicked(0))
        {
          scene.userCamera.arcballCamera.targetPosition = vec3(character.transform[3]) + vec3(0, 1, 0);
        }
      }
      if (selectedCharacter == i)
      {
        const float INDENT = 15.0f;
        ImGui::Indent(INDENT);
        ImGui::Text("Meshes: %zu", character.meshes.size());
        // show skeleton
        SkeletonRuntime &skeleton = character.skeleton;
        ImGui::Text("Skeleton Nodes: %zu", skeleton.names.size());
        for (size_t j = 0; j < skeleton.names.size(); j++)
        {
          // show text with tabs for hierarchy
          const std::string &name = skeleton.names[j];
          int depth = skeleton.hierarchyDepth[j];
          std::string tabs(depth, ' ');
          std::string label = tabs + name;
          if (ImGui::Selectable(label.c_str(), selectedNode == j))
          {
            selectedNode = j;
          }
        }
        ImGui::Unindent(INDENT);
      }
      ImGui::PopID();
    }
    if (selectedCharacter < scene.characters.size())
    {
      Character &character = scene.characters[selectedCharacter];
      if (selectedNode < character.skeleton.names.size())
      {
        glm::mat4 transform = character.transform * character.skeleton.worldTransform[selectedNode];
        manipulate_transform(transform, scene.userCamera);
        character.skeleton.worldTransform[selectedNode] = inverse(character.transform) * transform;
      }
      else
      {
        manipulate_transform(character.transform, scene.userCamera);
      }
    }
  }
  ImGui::End();
  if (selectedCharacter < scene.characters.size())
  {
    Character &character = scene.characters[selectedCharacter];
    if (selectedNode < character.skeleton.names.size())
    {
      glm::mat4 transform = character.transform * character.skeleton.worldTransform[selectedNode];
      // manipulate_transform(transform, scene.userCamera);
      character.skeleton.worldTransform[selectedNode] = inverse(character.transform) * transform;
    }
    else
    {
      // manipulate_transform(character.transform, scene.userCamera);
    }
    {
      const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

      ImGuiIO& io = ImGui::GetIO();
      ImGui::SetNextWindowSize(io.DisplaySize);
      ImGui::SetNextWindowPos(ImVec2(0, 0));

      ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
      ImGui::PushStyleColor(ImGuiCol_Border, 0);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

      ImGui::Begin("gizmo", NULL, flags);

      // vizualize skeleton
      ImDrawList* drawList = ImGui::GetWindowDrawList();
      // nice golden semi-transparent color for bones
      const auto boneColor = IM_COL32(255, 215, 0, 175);
      // directional indicator colors
      const auto xAxisColor = IM_COL32(255, 0, 0, 200);    // Red for X axis
      const auto yAxisColor = IM_COL32(0, 255, 0, 200);    // Green for Y axis
      const auto zAxisColor = IM_COL32(0, 120, 255, 200);  // Blue for Z axis
      
      // Draw bones with directional indicators
      for (size_t j = 0; j < character.skeleton.names.size(); j++)
      {
        const glm::mat4 &transform = character.transform * character.skeleton.worldTransform[j];
        int parent = character.skeleton.parents[j];
        if (parent >= 0) // from parent to child
        {
          const glm::mat4 &parentTransform = character.transform * character.skeleton.worldTransform[parent];
          
          const glm::vec3 jointPos = glm::vec3(transform[3]);
          const glm::vec3 parentJointPos = glm::vec3(parentTransform[3]);
          
          const glm::vec3 boneDirection = jointPos - parentJointPos;
          const float boneLength = glm::length(boneDirection);
          
          const glm::vec2 jointScreen = world_to_screen(scene.userCamera, jointPos);
          const glm::vec2 parentScreen = world_to_screen(scene.userCamera, parentJointPos);
          
          const glm::vec3 midpoint = (jointPos + parentJointPos) * 0.5f;
          const glm::vec2 midpointScreen = world_to_screen(scene.userCamera, midpoint);
          
          drawList->AddLine(
              ImVec2(parentScreen.x, parentScreen.y), 
              ImVec2(jointScreen.x, jointScreen.y), 
              boneColor, 
              2.5f
          );
          
          const float axisScale = boneLength * 0.15f; 
          
          glm::vec3 xAxis = glm::normalize(glm::vec3(transform[0])) * axisScale;
          glm::vec3 yAxis = glm::normalize(glm::vec3(transform[1])) * axisScale;
          glm::vec3 zAxis = glm::normalize(glm::vec3(transform[2])) * axisScale;
          
          glm::vec2 xAxisScreen = world_to_screen(scene.userCamera, jointPos + xAxis);
          glm::vec2 yAxisScreen = world_to_screen(scene.userCamera, jointPos + yAxis);
          glm::vec2 zAxisScreen = world_to_screen(scene.userCamera, jointPos + zAxis);
          
		  // Draw joint and axis
          drawList->AddCircleFilled(
              ImVec2(jointScreen.x, jointScreen.y), 
              4.0f, 
              boneColor
          );
          
          drawList->AddLine(
              ImVec2(jointScreen.x, jointScreen.y), 
              ImVec2(xAxisScreen.x, xAxisScreen.y), 
              xAxisColor, 
              1.5f
          );
          
          drawList->AddLine(
              ImVec2(jointScreen.x, jointScreen.y), 
              ImVec2(yAxisScreen.x, yAxisScreen.y), 
              yAxisColor, 
              1.5f
          );
          
          drawList->AddLine(
              ImVec2(jointScreen.x, jointScreen.y), 
              ImVec2(zAxisScreen.x, zAxisScreen.y), 
              zAxisColor, 
              1.5f
          );
          
          const float arrowSize = 4.0f;
          const glm::vec2 boneDir = glm::normalize(jointScreen - parentScreen);
          const glm::vec2 perpDir(-boneDir.y, boneDir.x);
          
          const glm::vec2 arrowBase = jointScreen - boneDir * arrowSize * 2.0f;
          const glm::vec2 arrowLeft = arrowBase - boneDir * arrowSize + perpDir * arrowSize;
          const glm::vec2 arrowRight = arrowBase - boneDir * arrowSize - perpDir * arrowSize;
          
          drawList->AddTriangleFilled(
              ImVec2(jointScreen.x, jointScreen.y),
              ImVec2(arrowLeft.x, arrowLeft.y),
              ImVec2(arrowRight.x, arrowRight.y),
              IM_COL32(80, 200, 120, 230) // Emerald Green
              //IM_COL32(34, 139, 34, 230) //Forest Green
              //IM_COL32(152, 255, 152, 230) //Mint Green
              //IM_COL32(0, 128, 128, 230) //Teal Green
          );
        }
        else
        {
          // Root node visualization
          const glm::vec3 rootPos = glm::vec3(transform[3]);
          const glm::vec2 rootScreen = world_to_screen(scene.userCamera, rootPos);
          
          drawList->AddCircleFilled(
              ImVec2(rootScreen.x, rootScreen.y), 
              6.0f, 
              IM_COL32(255, 255, 0, 200) //yellow for root
          );
          
		  // Draw root axis
          const float rootAxisScale = 0.1f;
          glm::vec3 xAxis = glm::normalize(glm::vec3(transform[0])) * rootAxisScale;
          glm::vec3 yAxis = glm::normalize(glm::vec3(transform[1])) * rootAxisScale;
          glm::vec3 zAxis = glm::normalize(glm::vec3(transform[2])) * rootAxisScale;
          
          glm::vec2 xAxisScreen = world_to_screen(scene.userCamera, rootPos + xAxis);
          glm::vec2 yAxisScreen = world_to_screen(scene.userCamera, rootPos + yAxis);
          glm::vec2 zAxisScreen = world_to_screen(scene.userCamera, rootPos + zAxis);
          
          drawList->AddLine(
              ImVec2(rootScreen.x, rootScreen.y), 
              ImVec2(xAxisScreen.x, xAxisScreen.y), 
              xAxisColor, 
              2.0f
          );
 
          drawList->AddLine(
              ImVec2(rootScreen.x, rootScreen.y), 
              ImVec2(yAxisScreen.x, yAxisScreen.y), 
              yAxisColor, 
              2.0f
          );
          
          drawList->AddLine(
              ImVec2(rootScreen.x, rootScreen.y), 
              ImVec2(zAxisScreen.x, zAxisScreen.y), 
              zAxisColor, 
              2.0f
          );
        }
		// Draw selected node indicator
        if (j == selectedNode)
        {
          const glm::vec3 selectedPos = glm::vec3(transform[3]);
          const glm::vec2 selectedScreen = world_to_screen(scene.userCamera, selectedPos);
          
          drawList->AddCircle(
              ImVec2(selectedScreen.x, selectedScreen.y), 
              8.0f, 
              IM_COL32(255, 255, 255, 255), 
              12, 
              2.0f
          );
        }
      }
      ImGui::End();
      ImGui::PopStyleVar();
      ImGui::PopStyleColor(2);
    }
  }
}

void render_imguizmo(ImGuizmo::OPERATION &mCurrentGizmoOperation, ImGuizmo::MODE &mCurrentGizmoMode)
{
  if (ImGui::Begin("gizmo window"))
  {
    if (ImGui::IsKeyPressed('Z'))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed('E'))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed('R')) // r Key
      mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
      mCurrentGizmoOperation = ImGuizmo::SCALE;

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
      if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
        mCurrentGizmoMode = ImGuizmo::LOCAL;
      ImGui::SameLine();
      if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
        mCurrentGizmoMode = ImGuizmo::WORLD;
    }
  }
  ImGui::End();
}

static void show_models(Scene &scene)
{
  if (ImGui::Begin("Models"))
  {
    static uint32_t selectedModel = -1u;
    for (size_t i = 0; i < scene.models.size(); i++)
    {
      const ModelAsset &model = scene.models[i];
      if (ImGui::Selectable(model.path.c_str(), selectedModel == i))
      {
        selectedModel = i;
      }
      if (selectedModel == i)
      {
        ImGui::Indent(15.0f);
        ImGui::Text("Path: %s", model.path.c_str());
        ImGui::Text("Meshes: %zu", model.meshes.size());
        for (size_t j = 0; j < model.meshes.size(); j++)
        {
          const MeshPtr &mesh = model.meshes[j];
          ImGui::Text("%s", mesh->name.c_str());
        }
        // show skeleton
        const SkeletonOffline &skeleton = model.skeleton;
        ImGui::Text("Skeleton Nodes: %zu", skeleton.names.size());
        for (size_t j = 0; j < skeleton.names.size(); j++)
        {
          // show text with tabs for hierarchy
          const std::string &name = skeleton.names[j];
          int depth = skeleton.hierarchyDepth[j];
          std::string tabs(depth, ' ');
          ImGui::Text("%s%s", tabs.c_str(), name.c_str());
        }
        ImGui::Unindent(15.0f);
      }
    }
  }
  ImGui::End();
}

void application_imgui_render(Scene &scene)
{
  render_imguizmo(mCurrentGizmoOperation, mCurrentGizmoMode);

  show_info();
  show_characters(scene);
  show_models(scene);
}