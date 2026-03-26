#version 400

const float pi = 3.14159265359;
const float eps = 0.0000001;

struct VsOutput
{
  vec3 EyespaceNormal;
  vec3 WorldPosition;
  vec2 UV;
  vec3 BoneColor;
};

uniform vec3 CameraPosition;
uniform vec3 LightDirection;
uniform vec3 AmbientLight;
uniform vec3 SunLight;
uniform float BoneInfluenceStrength = 3.5; 
uniform float SpecularStrength = 0.5;    
uniform float ShadowSoftness = 0.5;

in VsOutput vsOutput;
out vec4 FragColor;

uniform sampler2D mainTex;

vec3 AdvancedLighting(
  vec3 albedo,
  float shininess,
  float metallness,
  vec3 world_position,
  vec3 world_normal,
  vec3 light_dir,
  vec3 camera_pos)
{
  vec3 N = normalize(world_normal);
  vec3 L = normalize(-light_dir);
  vec3 V = normalize(camera_pos - world_position);
  vec3 H = normalize(L + V);
  vec3 R = reflect(-L, N);
  
  float NdotL = dot(N, L);
  float diffuseIntensity = smoothstep(-ShadowSoftness, ShadowSoftness, NdotL);
  
  float NdotH = max(dot(N, H), 0.0);
  float alpha = pow(2.0, 2.0 * (1.0 - shininess)); // Convert shininess to roughness
  float D = alpha * alpha / (pi * pow(NdotH * NdotH * (alpha * alpha - 1.0) + 1.0, 2.0));
  float specularIntensity = min(1.0, D * 0.25);
  
  float fresnelBase = 1.0 - max(0.0, dot(N, V));
  float fresnel = metallness + (1.0 - metallness) * pow(fresnelBase, 5.0);
  
  float rimFactor = pow(1.0 - max(0.0, dot(N, V)), 3.0) * 0.1;
  vec3 rimColor = SunLight * 0.5;
  
  vec3 diffuseColor = albedo * (AmbientLight + diffuseIntensity * SunLight);
  vec3 specularColor = fresnel * specularIntensity * SunLight * SpecularStrength;
  vec3 rimLighting = rimFactor * rimColor;
  
  return diffuseColor + specularColor + rimLighting;
}

vec3 BlendWithBoneColor(vec3 baseColor, vec3 boneColor, float influence) {
  // Bright bone color
  vec3 enhancedBoneColor = mix(boneColor, vec3(1.), 0.2);
  float baseLuma = dot(baseColor, vec3(0.3, 0.6, 0.1));
  vec3 result = mix(baseColor, enhancedBoneColor * baseLuma * 1.5, influence);
  
  return result;
}

void main()
{
  float shininess = 1.3;
  float metallness = 0.4;
  
  vec3 baseColor = texture(mainTex, vsOutput.UV).rgb;
  vec3 boneColor = vsOutput.BoneColor;
  
  vec3 litColor = AdvancedLighting(
    baseColor, 
    shininess, 
    metallness, 
    vsOutput.WorldPosition, 
    vsOutput.EyespaceNormal, 
    LightDirection, 
    CameraPosition
  );
  
  vec3 finalColor = BlendWithBoneColor(litColor, boneColor, BoneInfluenceStrength);
  finalColor = finalColor / (finalColor + vec3(1.0));
  finalColor = pow(finalColor, vec3(1.0/2.2)); 
  FragColor = vec4(finalColor, 1.0);
}