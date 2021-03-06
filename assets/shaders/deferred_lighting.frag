#version 450

layout(location = 0) in vec2 in_uvs;

layout(location = 0) out vec4 out_color;

const uint G_BUFFER_ALBEDO	= 0;
const uint G_BUFFER_POSITION	= 1;
const uint G_BUFFER_NORMAL	= 2;
const uint G_BUFFER_TOTAL	= 4;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput g_buffer_albedo;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput g_buffer_position;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput g_buffer_normal;

layout(set = 1, binding = 0) uniform samplerCube irradiance_cubemap;
layout(set = 2, binding = 0) uniform samplerCube prefiltered_environment;
layout(set = 3, binding = 0) uniform sampler2D integrate_lookup;
layout(set = 4, binding = 0) uniform sampler2DArray shadow_map;

//layout(set = 1, binding = 0) uniform samplerCube cubemap_sampler;

layout(push_constant) uniform Push_K
{
    vec4 light_direction;
    mat4 view_matrix;
    mat4 inverse_view_matrix;
    vec4 ws_view_direction;
} push_k;

layout(set = 5, binding = 0) uniform camera_information_t
{
    mat4 view;
    mat4 proj;

    mat4 shadow_view[4];
    mat4 shadow_proj[4];

    vec4 debug_vector;
    vec4 light_direction;
    mat4 inverse_view_matrix;
    vec4 view_direction;
    vec4 far_planes;
} camera_transforms;

const float PI = 3.14159265359;

float normal_distribution_ggx(vec3 vs_normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(vs_normal, halfway), 0.0);
    float a2minus1 = a2 - 1;
    float integral_den = ndoth * ndoth * a2minus1 + 1;

    float num = a2;
    float den = PI * integral_den * integral_den;

    return num / den;
}

float geometry_schlick_ggx(float ndotv, float roughness)
{
    float r = (roughness + 1);
    float k = (r * r) / 8;

    float num = ndotv;
    float den = ndotv * (1 - k) + k;

    return num / den;
}

float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness)
{
    float ndotv = max(dot(n, v), 0.0);
    float ndotl = max(dot(n, l), 0.0);

    return geometry_schlick_ggx(ndotv, roughness) * geometry_schlick_ggx(ndotl, roughness);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cos_theta, 5.0);
}  

vec3 fresnel_schlick(float cos_theta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}


const float MAP_SIZE = 2000.0;
const float PCF_COUNT = 1.0;
const float TRANSITION_DISTANCE = 20.0;
const float SHADOW_DISTANCE = 1000.0;

float get_shadow_light_factor(float dist, in vec4 shadow_coord, int layer)
{
    float total_texels = (PCF_COUNT * 2.0f + 1.0f) * (PCF_COUNT * 2.0f + 1.0f);

    dist = dist - (SHADOW_DISTANCE - TRANSITION_DISTANCE);
    dist = dist / TRANSITION_DISTANCE;
    dist = clamp(1.0 - dist, 0.0, 1.0);

    float texel_size = 1.0f / MAP_SIZE;
    float total = 0.0f;

    vec3 shadow_space_pos = shadow_coord.xyz / shadow_coord.w;
    shadow_space_pos.xy = shadow_space_pos.xy * 0.5 + 0.5;
    
    if (shadow_space_pos.z > -1.0 && shadow_space_pos.z < 1.0
	&& shadow_space_pos.x > 0.0 && shadow_space_pos.x < 1.0
	&& shadow_space_pos.y > 0.0 && shadow_space_pos.y < 1.0)
    {
	for (int x = int(-PCF_COUNT); x <= int(PCF_COUNT); ++x)
	{
	    for (int y = int(-PCF_COUNT); y <= int(PCF_COUNT); ++y)
	    {
		float object_nearest_light = texture(shadow_map, vec3(shadow_space_pos.xy + vec2(x, y) * texel_size, layer)).x;
		if (shadow_space_pos.z - 0.005 > object_nearest_light)
		{
		    total += 0.8f;
		}
	    }
	}
	total /= total_texels;
    }

    float light_factor = 1.0f - (total * dist);

    return light_factor;
}


// roughness of the material controlled in normal.a and the metalness in the position.a
vec4 pbr(void)
{
    vec4 albedo = subpassLoad(g_buffer_albedo);

    albedo.xyz = pow(albedo.xyz, vec3(2.2));
    vec4 gposition = subpassLoad(g_buffer_position);
    vec3 vs_position = gposition.xyz;
    
    float shadow_factor = albedo.a;

    vec4 gnormal = -subpassLoad(g_buffer_normal);
    vec3 vs_normal = gnormal.xyz;
    vec3 ws_normal = vec3(push_k.inverse_view_matrix * vec4(vs_normal, 0.0));
    float roughness = gnormal.a;
    float metallic = gposition.a;

    vec3 reflection_vector = reflect(-push_k.ws_view_direction.xyz, -ws_normal);

    const float MAX_REFLECTION_LOD = 4.0;

    vec3 prefiltered_color = textureLod(prefiltered_environment, reflection_vector, roughness * MAX_REFLECTION_LOD).rgb;
    
    vec3 radiance = vec3(24.47, 21.31, 20.79);
    vec3 ws_light = normalize(push_k.light_direction.xyz);
    //    ws_light.y *= 1.0;
    ws_light.xz *= -1.0f;
    vec3 light_vector = vec3(push_k.view_matrix * vec4(ws_light, 0.0));
    vec3 to_camera = normalize(vs_position);
    vec3 halfway = normalize(to_camera + light_vector);

    float n_dot_l = max(dot(vs_normal, light_vector), 0.0);

    // BRDF

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.xyz, metallic);
    
    vec3 irradiance = texture(irradiance_cubemap, ws_normal).rgb;
    
    vec3 F = fresnel_schlick_roughness(max(dot(vs_normal, to_camera), 0.0), F0, roughness);
    float D = normal_distribution_ggx(vs_normal, halfway, roughness);
    float G = geometry_smith(vs_normal, to_camera, light_vector, roughness);

    vec3 num = F * D * G;
    float den = 4 * max(dot(vs_normal, to_camera), 0.0) * max(dot(vs_normal, light_vector), 0.1);
    vec3 spec = num / max(den, 0.001);

    
    vec3 ks = F;
    vec3 kd = vec3(1.0) - ks;
    kd *= 1.0 - metallic;

    float substitute_shadow_factor = 1.0;
    if (n_dot_l < 0.6) substitute_shadow_factor = shadow_factor;
    
    vec3 result = (substitute_shadow_factor * kd * vec3(albedo) / PI + substitute_shadow_factor * spec) * radiance * n_dot_l;

    vec2 env_brdf = texture(integrate_lookup, vec2(max(dot(ws_normal.xyz, push_k.ws_view_direction.xyz), 0.0), roughness)).rg;
    vec3 specular = prefiltered_color * (F * env_brdf.x + env_brdf.y);
    
    vec3 diffuse = irradiance * albedo.rgb;
    vec3 ambient = (kd * diffuse + specular);

    //vec3 ambient = vec3(0.03) * vec3(albedo);

    result += ambient;
    
    ambient = pow(ambient, vec3(1.0 / 2.2));
    
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0 / 2.2));

    result = clamp(result, ambient, vec3(1.0));

    
    
    return vec4(result * shadow_factor, 1.0);
}

void
main(void)
{
    vec3 albedo_color = subpassLoad(g_buffer_albedo).rgb;
    vec3 vs_position = subpassLoad(g_buffer_position).rgb;
    vec3 vs_normal = subpassLoad(g_buffer_normal).rgb;

    out_color = vec4(albedo_color, 1.0);

    if (vs_normal.x > -10.0 && vs_normal.y > -10.0 && vs_normal.z > -10.0)
    {
	out_color = pbr();
    }
}
