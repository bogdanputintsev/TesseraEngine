#version 450

// =============================================
// Descriptors
// =============================================
// Set 0: Global data
layout(set = 0, binding = 0, std140) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float _pad0;
    int debug;
    int _pad1;
    int _pad2;
    int _pad3;
} globalUBO;

// Set 2: Material
layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D metallicMap;
layout(set = 2, binding = 3) uniform sampler2D roughnessMap;
layout(set = 2, binding = 4) uniform sampler2D aoMap;

// Set 3: Light sources
layout(set = 3, binding = 0, std140) uniform DirectionalLightUBO
{
    vec3 color;
    vec3 direction;
}   directionalLight;

// =============================================
// Input / Output
// =============================================
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTextureCoordinate;
layout(location = 2) in mat3 inTBN;

layout(location = 0) out vec4 outColor;
// =============================================
// Constants
// =============================================
const float AMBIENT_STRENGTH = 0.8;
const float PI = 3.1415926;
// =============================================
// Functions
// =============================================

//----------------------------------------------------------------------------------------------------------------------
// Normal Distribution Function (Trowbridge-Reitz GGX)
//----------------------------------------------------------------------------------------------------------------------
//      D(h) is the distribution function, or microfacet distribution function.
//      It describes:
//          - How likely it is that the microfacets on the surface are oriented in the direction of the half vector 
//      In other words, it is the “sharpness” of the glare.
//      The lower the roughness, the more microfacets are oriented in one direction → the glare is bright and narrow.
//      The higher the roughness, the more chaotic the microfacets → the glare is dull and smeared.
//----------------------------------------------------------------------------------------------------------------------
float NormalFunction (
    vec3 N,             // the normal vector
    vec3 H,             // the halfway vector between the surface normal and light direction
    float roughness)    // roughness
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

//----------------------------------------------------------------------------------------------------------------------
// Geometry function (Smith's Schlick-GGX)
//----------------------------------------------------------------------------------------------------------------------
//      The G function describes how much incoming or outgoing light gets blocked by the surface's microstructure.
//      Light can be:
//          - blocked before hitting a microfacet (called shadowing)
//          - blocked on the way out to the viewer (called masking)
//      This effect is most noticeable on rough surfaces, where tiny bumps and cracks interfere with the light’s path.
//----------------------------------------------------------------------------------------------------------------------
/**
 * @brief Calculates the Schlick-GGX approximation for just one direction (either view or light):
 *
 * @param NdotV Dot product between the surface normal and the view/light direction.
 *              Should be in the range [0, 1].
 * @param roughness Surface roughness parameter. Higher values simulate rougher surfaces.
 * @return The geometry attenuation factor for the given direction.
 */
float geometryFunctionOneDirectional(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

/**
 * @brief Calculates the Smith geometry function for both directions
 *
 * @param N Surface normal vector.
 * @param V View direction (from surface to camera).
 * @param L Light direction (from surface to light source).
 * @param roughness Surface roughness parameter.
 * @return The combined geometry attenuation factor for both directions.
 */
float GeometryFunction(vec3 N, vec3 V, vec3 L, float k)
{
    // Calculate geometry function of the angle between the normal and the camera.
    float NdotV = max(dot(N, V), 0.0);
    float ggx1 = geometryFunctionOneDirectional(NdotV, k);

    // Calculate geometry function of the angle between the normal and the light.
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometryFunctionOneDirectional(NdotL, k);

    return ggx1 * ggx2;
}

//----------------------------------------------------------------------------------------------------------------------
// Fresnel function (Fresnel-Schlick)
//----------------------------------------------------------------------------------------------------------------------
//      The Fresnel term, denoted F, describes how much light is reflected vs. refracted depending on the viewing angle.
//      The key idea:
//          - the closer your viewing angle is to grazing (looking along the surface), the more reflective the surface appears.
//      This explains why objects often look shinier at their edges.
//
//      @param cosTheta cos(theta), where theta is the angle between the view direction and halfway vector.
//      @param F0 base reflectivity color.
//----------------------------------------------------------------------------------------------------------------------
vec3 FresnelFunction(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//----------------------------------------------------------------------------------------------------------------------
// BRDF Function (Cook-Torrance BRDF)
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

// =============================================
// Main Function
// =============================================
void main() 
{
    // Sample textures
    vec3 albedo = texture(albedoMap, inTextureCoordinate).rgb;
    vec3 normal = normalize(texture(normalMap, inTextureCoordinate).rgb * 2.0 - 1.0);
    float metallic = texture(metallicMap, inTextureCoordinate).r;
    float roughness = texture(roughnessMap, inTextureCoordinate).r;
    float ao = texture(aoMap, inTextureCoordinate).r;
    
    if (globalUBO.debug == 1)
    {
        albedo = vec3(1.0);
    }
    
    vec3 N = normalize(inTBN * normal);
    vec3 V = normalize(globalUBO.cameraPos - inPosition);
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    vec3 L = normalize(directionalLight.direction);
    vec3 H = normalize(V + L);
    vec3 radiance = directionalLight.color;
    
    // BRDF
    float NDF = NormalFunction(N, H, roughness);
    float G   = GeometryFunction(N, V, L, roughness);
    vec3 F    = FresnelFunction(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    // add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    
    outColor = vec4(color, 1.0);
}
