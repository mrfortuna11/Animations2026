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

in VsOutput vsOutput;
out vec4 FragColor;
uniform sampler2D mainTex;

// Cook-Torrance BRDF
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
  float a      = roughness*roughness;
  float a2     = a*a;
  float NdotH  = max(dot(N, H), 0.0);
  float NdotH2 = NdotH*NdotH;

  float nom   = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = pi * denom * denom;

  return nom / max(denom, eps);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r*r) / 8.0;

  float nom   = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return nom / max(denom, eps);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
  return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

vec3 CookTorranceBRDF(
  vec3 albedo,
  float roughness,
  float metallic,
  vec3 world_position,
  vec3 N,
  vec3 L,
  vec3 camera_pos)
{
  vec3 V = normalize(camera_pos - world_position);
  
  L = -L;
  
  vec3 H = normalize(V + L);
  
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  
  float shadowTerm = pow(NdotL, 1.4);
  
  if (NdotL <= 0.0) {
    return albedo * AmbientLight * 0.6; 
  }
  vec3 F0 = vec3(0.04); //0.04 base
  F0 = mix(F0, albedo, metallic);
  
  // Normal Distribution Function (D): describes the distribution of microfacets
  float NDF = DistributionGGX(N, H, roughness);
  
  // Geometry Function (G): describes the self-shadowing of microfacets
  float G = GeometrySmith(N, V, L, roughness);
  
  // Fresnel Equation (F): describes reflection ratio based on angle
  vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
  
  vec3  numerator = NDF * G * F;
  float denominator = 4.0 * NdotV * NdotL + 0.0001;
  vec3  specular = numerator / denominator;
  
  vec3 kD = (vec3(1.0) - F * 0.75) * (1.0 - metallic * 0.85);
  
  vec3 directLighting = (kD * albedo + specular * 1.5) * SunLight * shadowTerm;
  
  float ambientOcclusion = pow(max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0), 0.5);
  vec3  ambient = albedo * AmbientLight * mix(0.7, 1.7, ambientOcclusion);
  
  return ambient + directLighting;
}

void main()
{
  float roughness = 0.35;
  float metallic = 0.4;
  
  vec3 albedo = texture(mainTex, vsOutput.UV).rgb;
  
  vec3 color = CookTorranceBRDF(
    albedo,
    roughness,
    metallic,
    vsOutput.WorldPosition,
    normalize(vsOutput.EyespaceNormal),
    normalize(LightDirection),
    CameraPosition
  );
  
  color = color * 2.3;  // Brighten the result
  color = color / (color + vec3(0.5));
  FragColor = vec4(color, 1.0);
}



// BONE INFLUENCE STRENGTH 

// #version 400

// const float pi = 3.14159265359;
// const float eps = 0.0000001;

// struct VsOutput
// {
//   vec3 EyespaceNormal;
//   vec3 WorldPosition;
//   vec2 UV;
//   vec3 BoneColor;
// };

// uniform vec3 CameraPosition;
// uniform vec3 LightDirection;
// uniform vec3 AmbientLight;
// uniform vec3 SunLight;
// uniform float BoneInfluenceStrength = 3.5; 
// uniform float SpecularStrength = 0.5;    
// uniform float ShadowSoftness = 0.5;

// in VsOutput vsOutput;
// out vec4 FragColor;

// uniform sampler2D mainTex;

// vec3 AdvancedLighting(
//   vec3 albedo,
//   float shininess,
//   float metallness,
//   vec3 world_position,
//   vec3 world_normal,
//   vec3 light_dir,
//   vec3 camera_pos)
// {
//   vec3 N = normalize(world_normal);
//   vec3 L = normalize(-light_dir);
//   vec3 V = normalize(camera_pos - world_position);
//   vec3 H = normalize(L + V);
//   vec3 R = reflect(-L, N);
  
//   float NdotL = dot(N, L);
//   float diffuseIntensity = smoothstep(-ShadowSoftness, ShadowSoftness, NdotL);
  
//   float NdotH = max(dot(N, H), 0.0);
//   float alpha = pow(2.0, 2.0 * (1.0 - shininess)); // Convert shininess to roughness
//   float D = alpha * alpha / (pi * pow(NdotH * NdotH * (alpha * alpha - 1.0) + 1.0, 2.0));
//   float specularIntensity = min(1.0, D * 0.25);
  
//   float fresnelBase = 1.0 - max(0.0, dot(N, V));
//   float fresnel = metallness + (1.0 - metallness) * pow(fresnelBase, 5.0);
  
//   float rimFactor = pow(1.0 - max(0.0, dot(N, V)), 3.0) * 0.1;
//   vec3 rimColor = SunLight * 0.5;
  
//   vec3 diffuseColor = albedo * (AmbientLight + diffuseIntensity * SunLight);
//   vec3 specularColor = fresnel * specularIntensity * SunLight * SpecularStrength;
//   vec3 rimLighting = rimFactor * rimColor;
  
//   return diffuseColor + specularColor + rimLighting;
// }

// vec3 BlendWithBoneColor(vec3 baseColor, vec3 boneColor, float influence) {
//   // Bright bone color
//   vec3 enhancedBoneColor = mix(boneColor, vec3(1.), 0.2);
//   float baseLuma = dot(baseColor, vec3(0.3, 0.6, 0.1));
//   vec3 result = mix(baseColor, enhancedBoneColor * baseLuma * 1.5, influence);
  
//   return result;
// }

// void main()
// {
//   float shininess = 1.3;
//   float metallness = 0.4;
  
//   vec3 baseColor = texture(mainTex, vsOutput.UV).rgb;
//   vec3 boneColor = vsOutput.BoneColor;
  
//   vec3 litColor = AdvancedLighting(
//     baseColor, 
//     shininess, 
//     metallness, 
//     vsOutput.WorldPosition, 
//     vsOutput.EyespaceNormal, 
//     LightDirection, 
//     CameraPosition
//   );
  
//   vec3 finalColor = BlendWithBoneColor(litColor, boneColor, BoneInfluenceStrength);
//   finalColor = finalColor / (finalColor + vec3(1.0));
//   finalColor = pow(finalColor, vec3(1.0/2.2)); 
//   FragColor = vec4(finalColor, 1.0);
// }



// OLD VERSION Fong

// #version 400

// struct VsOutput
// {
//   vec3 EyespaceNormal;
//   vec3 WorldPosition;
//   vec2 UV;
//   vec3 BoneColor;
// };

// uniform vec3 CameraPosition;
// uniform vec3 LightDirection;
// uniform vec3 AmbientLight;
// uniform vec3 SunLight;

// in VsOutput vsOutput;
// out vec4 FragColor;

// uniform sampler2D mainTex;

// vec3 LightedColor(
//   vec3 color,
//   float shininess,
//   float metallness,
//   vec3 world_position,
//   vec3 world_normal,
//   vec3 light_dir,
//   vec3 camera_pos)
// {
//   vec3 W = normalize(camera_pos - world_position);
//   vec3 E = reflect(light_dir, world_normal);
//   float df = max(0.0, dot(world_normal, -light_dir));
//   float sf = max(0.0, dot(E, W));
//   sf = pow(sf, shininess);
//   return color * (AmbientLight + df * SunLight) + vec3(1,1,1) * sf * metallness;
// }

// void main()
// {
//   float shininess = 1.3;
//   float metallness = 0.4;
//   vec3 color = texture(mainTex, vsOutput.UV).rgb ;
//   color = LightedColor(color, shininess, metallness, vsOutput.WorldPosition, vsOutput.EyespaceNormal, LightDirection, CameraPosition);
//   // FragColor = vec4(vsOutput.BoneColor, 1.0); // vec4(color, 1.0);
//   FragColor = vec4(color, 1.0);
// }