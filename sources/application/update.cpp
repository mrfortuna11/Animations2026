#include "scene.h"
#include <ozz/animation/runtime/local_to_model_job.h>

void application_update(Scene &scene)
{
  arcball_camera_update(
    scene.userCamera.arcballCamera,
    scene.userCamera.transform,
    engine::get_delta_time());

  for (Character& character : scene.characters)
  {
    AnimationContext& animationContext = character.animationContext;

    // Animation sampling
    if (animationContext.currentAnimation != nullptr)
    {
      ozz::animation::SamplingJob samplingJob;
      samplingJob.ratio = animationContext.currentProgress;
      assert(0.f <= samplingJob.ratio && samplingJob.ratio <= 1.f);
      assert(animationContext.currentAnimation->num_tracks() == animationContext.skeleton->num_joints());
      samplingJob.animation = animationContext.currentAnimation;
      samplingJob.context = animationContext.samplingCache.get();
      samplingJob.output = ozz::make_span(animationContext.localTransforms);

      assert(samplingJob.Validate());
      const bool success = samplingJob.Run();
      assert(success);

      animationContext.currentProgress += engine::get_delta_time() / animationContext.currentAnimation->duration();
      if (animationContext.currentProgress > 1.f)
        animationContext.currentProgress = 0.f;
    }
    else
    {
      auto tPose = animationContext.skeleton->joint_rest_poses();
      animationContext.localTransforms.assign(tPose.begin(), tPose.end());
    }

    ozz::animation::LocalToModelJob localToModelJob;
    localToModelJob.skeleton = animationContext.skeleton;
    localToModelJob.input = ozz::make_span(animationContext.localTransforms);
    localToModelJob.output = ozz::make_span(animationContext.worldTransforms);

    ozz::math::Float4x4 root;
    memcpy(&root, &character.transform, sizeof(root));
    localToModelJob.root = &root;

    assert(localToModelJob.Validate());
    const bool success = localToModelJob.Run();
    assert(success);
  }
}