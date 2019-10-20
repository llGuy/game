#include "ui.hpp"
#include "script.hpp"
#include "world.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/projection.hpp>

#include "graphics.hpp"

#include "core.hpp"

#include "game.hpp"

#define MAX_ENTITIES_UNDER_TOP 10
#define MAX_ENTITIES_UNDER_PLANET 150

constexpr float32_t PI = 3.14159265359f;

// To initialize with initialize translation unit function
global_var struct entities_t *g_entities;
global_var struct voxel_chunks_t *g_voxel_chunks;

enum matrix4_mul_vec3_with_translation_flag { WITH_TRANSLATION, WITHOUT_TRANSLATION, TRANSLATION_DONT_CARE };

// Initialization
internal_function void hard_initialize_chunks(void);
void initialize_chunk(voxel_chunk_t *chunk, vector3_t chunk_position, ivector3_t chunk_coord);

// Vector space operations
internal_function vector3_t get_voxel_world_origin(void);
internal_function bool is_voxel_coord_within_chunk(const ivector3_t &coord, uint32_t edge_length = VOXEL_CHUNK_EDGE_LENGTH);
internal_function vector3_t ws_to_xs(const vector3_t &ws_position);
internal_function int32_t convert_3d_to_1d_index(uint32_t x, uint32_t y, uint32_t z, uint32_t edge_length);

// Voxel coordinate fetching / system
voxel_chunk_t **get_voxel_chunk(int32_t index);
internal_function voxel_chunk_t *get_chunk_encompassing_point(const vector3_t &xs_position);
internal_function ivector3_t get_voxel_coord(const vector3_t &xs_position);
internal_function ivector3_t get_voxel_coord(const ivector3_t &xs_position);

// Modifications
internal_function void terraform(const ivector3_t &xs_voxel_coord, uint32_t voxel_radius, bool destructive, float32_t dt);
internal_function void construct_plane(const vector3_t &ws_plane_origin, float32_t radius);
internal_function void construct_sphere(const vector3_t &ws_sphere_position, float32_t radius);
internal_function void ray_cast_terraform(const vector3_t &ws_position, const vector3_t &ws_direction, float32_t max_reach_distance, float32_t dt, uint32_t surface_level, bool destructive);

// Rendering / Mesh / GPU operations
internal_function void push_vertex_to_triangle_array(uint8_t v0, uint8_t v1, vector3_t *vertices, voxel_chunk_t *chunk, uint8_t *voxel_values, uint8_t surface_level);
internal_function uint8_t chunk_edge_voxel_value(voxel_chunk_t *chunk, uint32_t x, uint32_t y, uint32_t z, bool *doesnt_exist);
internal_function void update_chunk_mesh_struct_vertex_count(voxel_chunk_t *chunk);
internal_function void update_chunk_mesh_voxel_pair(uint8_t *voxel_values, voxel_chunk_t *chunk, uint32_t x, uint32_t y, uint32_t z, uint8_t surface_level);
void update_chunk_mesh(voxel_chunk_t *chunk, uint8_t surface_level);
void ready_chunk_for_gpu_sync(voxel_chunk_t *chunk);
void push_chunk_to_render_queue(voxel_chunk_t *chunk);
internal_function void sync_gpu_with_chunk_state(gpu_command_queue_t *queue);
internal_function void dbg_render_chunk_edges(gpu_command_queue_t *queue, uniform_group_t *transforms_ubo);

// Math
internal_function float32_t lerp(float32_t a, float32_t b, float32_t x);
internal_function vector3_t interpolate(const vector3_t &a, const vector3_t &b, float32_t x);
internal_function float32_t squared(float32_t f);
internal_function float32_t distance_squared(const vector3_t &dir);

// Collision
struct movement_axes_t
{
    vector3_t right;
    vector3_t up;
    vector3_t forward;
};
internal_function movement_axes_t compute_movement_axes(const vector3_t &view_direction, const vector3_t &up);
enum collision_primitive_type_t { CPT_FACE, CPT_EDGE, CPT_VERTEX };
struct collision_t
{
    collision_primitive_type_t primitive_type;

    uint8_t detected: 1;
    // Detected a collision but now "slid" into the air
    uint8_t is_currently_in_air: 1;
    uint8_t under_terrain: 1;

    vector3_t es_velocity;
    vector3_t es_contact_point;
    vector3_t es_at;
    vector3_t es_normal;
    float32_t es_distance;
};
internal_function void push_collision_vertex(uint8_t v0, uint8_t v1, vector3_t *vertices, uint8_t *voxel_values, uint8_t surface_level, vector3_t *dst_array, uint32_t *count);
internal_function void push_collision_triangles_vertices(uint8_t *voxel_values, uint32_t x, uint32_t y, uint32_t z, uint8_t surface_level, vector3_t *dst_array, uint32_t *count, uint32_t max);
internal_function bool32_t is_point_in_triangle(const vector3_t &point, const vector3_t &tri_point_a, const vector3_t &tri_point_b, const vector3_t &tri_point_c);
internal_function bool get_smallest_root(float32_t a, float32_t b, float32_t c, float32_t max_r, float32_t *root);
internal_function float32_t get_plane_constant(const vector3_t &plane_point, const vector3_t &plane_normal);
internal_function void check_collision_with_vertex(const vector3_t &es_sphere_velocity, const vector3_t &es_sphere_position, const vector3_t &es_vertex, const vector3_t &es_surface_normal, collision_t *collision);
internal_function void check_collision_with_edge(const vector3_t &es_sphere_velocity, const vector3_t &es_sphere_position, const vector3_t &es_vertex_a, const vector3_t &es_vertex_b, const vector3_t &es_surface_normal, collision_t *collision);
internal_function void collide_with_triangle(vector3_t *triangle_vertices, const vector3_t &es_center, const vector3_t &es_velocity, collision_t *closest);
internal_function collision_t collide(const vector3_t &ws_center, const vector3_t &ws_size, const vector3_t &ws_velocity, uint32_t recurse_depth, collision_t previous_collision);

// Entities
internal_function player_t *get_main_player(void);
internal_function player_t *get_player(const constant_string_t &name);
player_t *get_player(player_handle_t v);

internal_function void push_player_to_queue(player_t *e_ptr, mesh_t *mesh, gpu_material_submission_queue_t *queue);
internal_function void push_player_to_animated_queue(player_t *player);
internal_function void push_player_to_rolling_queue(player_t *player);
internal_function void update_animation_gpu_data(gpu_command_queue_t *queue);

player_t construct_player(player_create_info_t *info);
internal_function player_handle_t add_player(const player_t &e);
internal_function void make_player_main(player_handle_t player_handle);

internal_function void update_camera_component(camera_component_t *camera, player_t *player, float32_t dt);
internal_function void update_animation_component(animation_component_t *animation, player_t *player, float32_t dt);
internal_function void update_rendering_component(rendering_component_t *rendering, player_t *player, float32_t dt);
internal_function void update_terraform_power_component(terraform_power_component_t *terraform_power, player_t *player, float32_t dt);
internal_function void update_standing_player_physics(physics_component_t *component, player_t *e, uint32_t *action_flags, float32_t dt);
internal_function void update_rolling_player_physics(physics_component_t *component, player_t *e, uint32_t *action_flags, float32_t dt);
internal_function void update_not_physically_affected_player(physics_component_t *component, player_t *e, uint32_t *action_flags, float32_t dt);
internal_function void update_physics_component(physics_component_t *physics, player_t *player, float32_t dt);
internal_function void update_entities(float32_t dt, application_type_t app_type);

internal_function void initialize_entities_graphics_data(VkCommandPool *cmdpool, input_state_t *input_state);
internal_function void initialize_players(input_state_t *input_state, application_type_t app_type);
internal_function void render_world(uint32_t image_index, uint32_t current_frame, gpu_command_queue_t *queue);

internal_function int32_t lua_get_player_position(lua_State *state);
internal_function int32_t lua_set_player_position(lua_State *state);
internal_function int32_t lua_toggle_collision_box_render(lua_State *state);
internal_function int32_t lua_toggle_collision_edge_render(lua_State *state);
internal_function int32_t lua_toggle_sphere_collision_triangles_render(lua_State *state);
internal_function int32_t lua_render_player_direction_information(lua_State *state);
internal_function int32_t lua_set_veclocity_in_view_direction(lua_State *state);
internal_function int32_t lua_get_player_ts_view_direction(lua_State *state);
internal_function int32_t lua_stop_simulation(lua_State *state);
internal_function int32_t lua_load_mesh(lua_State *state);
internal_function int32_t lua_load_model_information_for_mesh(lua_State *state);
internal_function int32_t lua_load_skeleton(lua_State *state);
internal_function int32_t lua_load_animations(lua_State *state);
internal_function int32_t lua_initialize_player(lua_State *state);
internal_function int32_t lua_attach_rendering_component(lua_State *state);
internal_function int32_t lua_attach_animation_component(lua_State *state);
internal_function int32_t lua_attach_physics_component(lua_State *state);
internal_function int32_t lua_attach_camera_component(lua_State *state);
internal_function int32_t lua_bind_player_to_3d_output(lua_State *state);
internal_function int32_t lua_go_down(lua_State *state);
internal_function int32_t lua_placeholder_c_out(lua_State *state) { return(0); }
internal_function int32_t lua_reinitialize(lua_State *state);

internal_function void entry_point(void);
void hard_initialize_world(input_state_t *input_state, VkCommandPool *cmdpool, application_type_t app_type, application_mode_t app_mode);
void initialize_world(input_state_t *input_state, VkCommandPool *cmdpool, application_type_t app_type, application_mode_t app_mode);
internal_function void clean_up_entities(void);
void clean_up_world_data(void);
void make_world_data(void /* Some kind of state */);
void update_network_world_state(void);
void sync_gpu_memory_with_world_state(gpu_command_queue_t *cmdbuf, uint32_t image_index);
void handle_all_input(input_state_t *input_state, float32_t dt, element_focus_t focus);
void update_world(input_state_t *input_state, float32_t dt, uint32_t image_index, uint32_t current_frame, gpu_command_queue_t *cmdbuf, application_type_t app_type, element_focus_t focus);
void handle_main_player_mouse_movement(player_t *e, uint32_t *action_flags, input_state_t *input_state, float32_t dt);
void handle_main_player_mouse_button_input(player_t *e, uint32_t *action_flags, input_state_t *input_state, float32_t dt);
void handle_main_player_keyboard_input(player_t *e, uint32_t *action_flags, physics_component_t *e_physics, input_state_t *input_state, float32_t dt);
void handle_main_player_action(input_state_t *input_state, float32_t dt);
void handle_world_input(input_state_t *input_state, float32_t dt);
void handle_input_debug(input_state_t *input_state, float32_t dt);
void destroy_world(void);




// ******************************** Voxel code ********************************

// Initialize rendering data, stuff that should only be initialize at the beginning of the game running in general
internal_function void hard_initialize_chunks(void)
{        
    g_voxel_chunks->chunk_model.attribute_count = 1;
    g_voxel_chunks->chunk_model.attributes_buffer = (VkVertexInputAttributeDescription *)allocate_free_list(sizeof(VkVertexInputAttributeDescription));
    g_voxel_chunks->chunk_model.binding_count = 1;
    g_voxel_chunks->chunk_model.bindings = (model_binding_t *)allocate_free_list(sizeof(model_binding_t));

    model_binding_t *binding = g_voxel_chunks->chunk_model.bindings;
    binding->begin_attributes_creation(g_voxel_chunks->chunk_model.attributes_buffer);

    // There is only one attribute for now
    binding->push_attribute(0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vector3_t));

    binding->end_attributes_creation();

    g_voxel_chunks->chunk_pipeline = g_pipeline_manager->add("pipeline.chunk_points"_hash);
    graphics_pipeline_t *voxel_pipeline = g_pipeline_manager->get(g_voxel_chunks->chunk_pipeline);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        render_pass_handle_t dfr_render_pass = g_render_pass_manager->get_handle("render_pass.deferred_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/voxel_point.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/voxel_point.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash));
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT };
        shader_blend_states_t blending(false, false, false, false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_POLYGON_MODE_POINT,
                                    VK_CULL_MODE_NONE, layouts, push_k, get_backbuffer_resolution(), blending, &g_voxel_chunks->chunk_model,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(dfr_render_pass), 0, info);
        voxel_pipeline->info = info;
        make_graphics_pipeline(voxel_pipeline);
    }

    g_voxel_chunks->chunk_mesh_pipeline = g_pipeline_manager->add("pipeline.chunk_mesh"_hash);
    graphics_pipeline_t *voxel_mesh_pipeline = g_pipeline_manager->get(g_voxel_chunks->chunk_mesh_pipeline);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        render_pass_handle_t dfr_render_pass = g_render_pass_manager->get_handle("render_pass.deferred_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/voxel_mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/voxel_mesh.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
                                 shader_module_info_t{"shaders/SPV/voxel_mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash),
                                         g_uniform_layout_manager->get_handle("descriptor_set_layout.2D_sampler_layout"_hash));
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT };
        shader_blend_states_t blending(false, false, false, false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                    VK_CULL_MODE_NONE, layouts, push_k, get_backbuffer_resolution(), blending, &g_voxel_chunks->chunk_model,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(dfr_render_pass), 0, info);
        voxel_mesh_pipeline->info = info;
        make_graphics_pipeline(voxel_mesh_pipeline);
    }

    g_voxel_chunks->chunk_mesh_shadow_pipeline = g_pipeline_manager->add("pipeline.chunk_mesh_shadow"_hash);
    graphics_pipeline_t *voxel_mesh_shadow_pipeline = g_pipeline_manager->get(g_voxel_chunks->chunk_mesh_shadow_pipeline);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        auto shadow_display = get_shadow_display();
        VkExtent2D shadow_extent {shadow_display.shadowmap_w, shadow_display.shadowmap_h};
        render_pass_handle_t shadow_render_pass = g_render_pass_manager->get_handle("render_pass.shadow_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/voxel_mesh_shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/voxel_mesh_shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash));
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT};
        shader_blend_states_t blending(false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_DEPTH_BIAS, VK_DYNAMIC_STATE_VIEWPORT);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                    VK_CULL_MODE_NONE, layouts, push_k, shadow_extent, blending, &g_voxel_chunks->chunk_model,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(shadow_render_pass), 0, info);
        voxel_mesh_shadow_pipeline->info = info;
        make_graphics_pipeline(voxel_mesh_shadow_pipeline);
    }

    g_voxel_chunks->dbg_chunk_edge_pipeline = g_pipeline_manager->add("pipeline.dbg_chunk_edge"_hash);
    auto *dbg_chunk_edge_pipeline = g_pipeline_manager->get(g_voxel_chunks->dbg_chunk_edge_pipeline);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        render_pass_handle_t dfr_render_pass = g_render_pass_manager->get_handle("render_pass.deferred_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/hitbox_render.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/hitbox_render.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash));
        shader_pk_data_t push_k = {240, 0, VK_SHADER_STAGE_VERTEX_BIT};
        shader_blend_states_t blending(false, false, false, false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_LINE,
                                    VK_CULL_MODE_NONE, layouts, push_k, get_backbuffer_resolution(), blending, nullptr,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(dfr_render_pass), 0, info);
        dbg_chunk_edge_pipeline->info = info;
        make_graphics_pipeline(dbg_chunk_edge_pipeline);
    }

    g_voxel_chunks->gpu_queue = make_gpu_material_submission_queue(20 * 20 * 20, VK_SHADER_STAGE_VERTEX_BIT, VK_COMMAND_BUFFER_LEVEL_PRIMARY, get_global_command_pool());
}


internal_function vector3_t get_voxel_world_origin(void)
{
    return -vector3_t((float32_t)g_voxel_chunks->grid_edge_size / 2.0f) * (float32_t)(VOXEL_CHUNK_EDGE_LENGTH) * g_voxel_chunks->size;
}


internal_function bool is_voxel_coord_within_chunk(const ivector3_t &coord, uint32_t edge_length)
{
    return(coord.x >= 0 && coord.x < edge_length &&
           coord.y >= 0 && coord.y < edge_length &&
           coord.z >= 0 && coord.z < edge_length);
}


// XS = voxel space (VS is view space)
internal_function vector3_t ws_to_xs(const vector3_t &ws_position)
{
    vector3_t from_origin = ws_position - get_voxel_world_origin();

    vector3_t xs_sized = from_origin / g_voxel_chunks->size;

    return xs_sized;
}


internal_function int32_t convert_3d_to_1d_index(uint32_t x, uint32_t y, uint32_t z, uint32_t edge_length)
{
    if (is_voxel_coord_within_chunk(ivector3_t(x, y, z), edge_length))
    {
        return(z * (edge_length * edge_length) + y * edge_length + x);
    }
    else
    {
        return -1;
    }
}


// CS = chunk space
internal_function voxel_chunk_t *get_chunk_encompassing_point(const vector3_t &xs_position)
{
    ivector3_t rounded = ivector3_t(glm::round(xs_position));

    ivector3_t chunk_coord = rounded / (VOXEL_CHUNK_EDGE_LENGTH);

    return(*get_voxel_chunk(convert_3d_to_1d_index(chunk_coord.x, chunk_coord.y, chunk_coord.z, g_voxel_chunks->grid_edge_size)));
    
    ivector3_t cs_voxel_coord = ivector3_t(rounded.x % VOXEL_CHUNK_EDGE_LENGTH, rounded.y % (VOXEL_CHUNK_EDGE_LENGTH), rounded.z % (VOXEL_CHUNK_EDGE_LENGTH));
}


internal_function ivector3_t get_voxel_coord(const vector3_t &xs_position)
{
    ivector3_t rounded = ivector3_t(glm::round(xs_position));
    ivector3_t cs_voxel_coord = ivector3_t(rounded.x % VOXEL_CHUNK_EDGE_LENGTH, rounded.y % (VOXEL_CHUNK_EDGE_LENGTH), rounded.z % (VOXEL_CHUNK_EDGE_LENGTH));
    return(cs_voxel_coord);
}


internal_function ivector3_t get_voxel_coord(const ivector3_t &xs_position)
{
    ivector3_t cs_voxel_coord = ivector3_t(xs_position.x % VOXEL_CHUNK_EDGE_LENGTH, xs_position.y % (VOXEL_CHUNK_EDGE_LENGTH), xs_position.z % (VOXEL_CHUNK_EDGE_LENGTH));
    return(cs_voxel_coord);
}


internal_function void terraform(const ivector3_t &xs_voxel_coord, uint32_t voxel_radius, bool destructive, float32_t dt)
{
    ivector3_t voxel_coord = xs_voxel_coord;
    voxel_chunk_t *chunk = get_chunk_encompassing_point(voxel_coord);
    ready_chunk_for_gpu_sync(chunk);
    
    float32_t coefficient = (destructive ? -1.0f : 1.0f);
    
    float_t radius = (float32_t)voxel_radius;
    
    float32_t radius_squared = radius * radius;
    
    ivector3_t bottom_corner = voxel_coord - ivector3_t((uint32_t)radius);

    uint32_t diameter = (uint32_t)radius * 2 + 1;

    for (uint32_t z = 0; z < diameter; ++z)
    {
        for (uint32_t y = 0; y < diameter; ++y)
        {
            for (uint32_t x = 0; x < diameter; ++x)
            {
                vector3_t v_f = vector3_t(x, y, z) + vector3_t(bottom_corner);
                vector3_t diff = v_f - vector3_t(voxel_coord);
                float32_t real_distance_squared = glm::dot(diff, diff);
                if (real_distance_squared <= radius_squared)
                {
                    ivector3_t cs_vcoord = ivector3_t(v_f) - chunk->xs_bottom_corner;

                    if (is_voxel_coord_within_chunk(cs_vcoord))
                    {
                        uint8_t *voxel = &chunk->voxels[(uint32_t)cs_vcoord.x][(uint32_t)cs_vcoord.y][(uint32_t)cs_vcoord.z];

                        float32_t proportion = 1.0f - (real_distance_squared / radius_squared);
                        
                        int32_t current_voxel_value = (int32_t)*voxel;
                        int32_t new_value = (int32_t)(proportion * coefficient * dt * 700.0f) + current_voxel_value;

                        if (new_value > 255)
                        {
                            *voxel = 255;
                        }
                        else if (new_value < 0)
                        {
                            *voxel = 0;
                        }
                        else
                        {
                            *voxel = (uint8_t)new_value;
                        }
                    }
                    else
                    {
                        voxel_chunk_t *new_chunk = get_chunk_encompassing_point(ivector3_t(v_f));
                        
                        if (new_chunk)
                        {
                            chunk = new_chunk;
                            ready_chunk_for_gpu_sync(chunk);
                            cs_vcoord = ivector3_t(v_f) - chunk->xs_bottom_corner;
                        
                            uint8_t *voxel = &chunk->voxels[(uint32_t)cs_vcoord.x][(uint32_t)cs_vcoord.y][(uint32_t)cs_vcoord.z];

                            float32_t proportion = 1.0f - (real_distance_squared / radius_squared);
                            int32_t current_voxel_value = (int32_t)*voxel;
                            int32_t new_value = (int32_t)(proportion * coefficient * dt * 700.0f) + current_voxel_value;

                            if (new_value > 255)
                            {
                                *voxel = 255;
                            }
                            else if (new_value < 0)
                            {
                                *voxel = 0;
                            }
                            else
                            {
                                *voxel = (uint8_t)new_value;
                            }
                        }
                    }
                }
            }
        }
    }

    ready_chunk_for_gpu_sync(chunk);
}


internal_function void construct_plane(const vector3_t &ws_plane_origin, float32_t radius)
{
    vector3_t xs_plane_origin = ws_to_xs(ws_plane_origin);

    voxel_chunk_t *chunk = get_chunk_encompassing_point(xs_plane_origin);
    ready_chunk_for_gpu_sync(chunk);
    
    ivector3_t plane_origin = ivector3_t(xs_plane_origin);

    radius /= g_voxel_chunks->size;
    radius = glm::round(radius);
    
    ivector3_t bottom_corner = plane_origin - ivector3_t((uint32_t)radius, 0, (uint32_t)radius);

    uint32_t diameter = (uint32_t)radius * 2 + 1;

    uint32_t y = 0;
    for (uint32_t z = 0; z < diameter; ++z)
    {
        for (uint32_t x = 0; x < diameter; ++x)
        {
            vector3_t v_f = vector3_t(x, y, z) + vector3_t(bottom_corner);

            ivector3_t cs_vcoord = ivector3_t(v_f) - chunk->xs_bottom_corner;

            if (is_voxel_coord_within_chunk(cs_vcoord))
            {
                chunk->voxels[(uint32_t)cs_vcoord.x][(uint32_t)cs_vcoord.y][(uint32_t)cs_vcoord.z] = 255;
            }
            else
            {
                chunk = get_chunk_encompassing_point(ivector3_t(v_f));
                ready_chunk_for_gpu_sync(chunk);
                cs_vcoord = ivector3_t(v_f) - chunk->xs_bottom_corner;

                chunk->voxels[(uint32_t)cs_vcoord.x][(uint32_t)cs_vcoord.y][(uint32_t)cs_vcoord.z] = 255;
            }
        }
    }
}


internal_function void construct_sphere(const vector3_t &ws_sphere_position, float32_t radius)
{
    vector3_t xs_sphere_position = ws_to_xs(ws_sphere_position);
    
    voxel_chunk_t *chunk = get_chunk_encompassing_point(xs_sphere_position);
    ready_chunk_for_gpu_sync(chunk);
    
    ivector3_t sphere_center = xs_sphere_position;

    radius /= g_voxel_chunks->size;
    radius = glm::round(radius);
    
    float32_t radius_squared = radius * radius;
    
    ivector3_t bottom_corner = sphere_center - ivector3_t((uint32_t)radius);

    uint32_t diameter = (uint32_t)radius * 2 + 1;

    for (uint32_t z = 0; z < diameter; ++z)
    {
        for (uint32_t y = 0; y < diameter; ++y)
        {
            for (uint32_t x = 0; x < diameter; ++x)
            {
                vector3_t v_f = vector3_t(x, y, z) + vector3_t(bottom_corner);
                vector3_t diff = v_f - vector3_t(sphere_center);
                float32_t real_distance_squared = glm::dot(diff, diff);

                if (real_distance_squared <= radius_squared)
                {
                    ivector3_t cs_vcoord = ivector3_t(v_f) - chunk->xs_bottom_corner;

                    if (is_voxel_coord_within_chunk(cs_vcoord))
                    {
                        float32_t proportion = 1.0f - (real_distance_squared / radius_squared);
                        chunk->voxels[(uint32_t)cs_vcoord.x][(uint32_t)cs_vcoord.y][(uint32_t)cs_vcoord.z] = (uint32_t)((proportion) * 255.0f);
                    }
                    else
                    {
                        chunk = get_chunk_encompassing_point(ivector3_t(v_f));
                        ready_chunk_for_gpu_sync(chunk);
                        cs_vcoord = ivector3_t(v_f) - chunk->xs_bottom_corner;

                        float32_t proportion = 1.0f - (real_distance_squared / radius_squared);
                        chunk->voxels[(uint32_t)cs_vcoord.x][(uint32_t)cs_vcoord.y][(uint32_t)cs_vcoord.z] = (uint32_t)((proportion) * 255.0f);
                    }
                }
            }
        }
    }
}


internal_function void ray_cast_terraform(const vector3_t &ws_position, const vector3_t &ws_direction, float32_t max_reach_distance, float32_t dt, uint32_t surface_level, bool destructive)
{
    vector3_t ray_start_position = ws_to_xs(ws_position);
    vector3_t current_ray_position = ray_start_position;
    vector3_t ray_direction = ws_direction;
    max_reach_distance /= g_voxel_chunks->size;
    float32_t ray_step_size = max_reach_distance / 10.0f;
    float32_t max_reach_distance_squared = max_reach_distance * max_reach_distance;
    
    for (; glm::dot(current_ray_position - ray_start_position, current_ray_position - ray_start_position) < max_reach_distance_squared; current_ray_position += ray_step_size * ray_direction)
    {
        voxel_chunk_t *chunk = get_chunk_encompassing_point(current_ray_position);

        if (chunk)
        {
            ivector3_t voxel_coord = get_voxel_coord(current_ray_position);

            if (chunk->voxels[voxel_coord.x][voxel_coord.y][voxel_coord.z] > surface_level)
            {
                terraform(ivector3_t(current_ray_position), 2, destructive, dt);
                break;
            }
        }
    }
}


void push_chunk_to_render_queue(voxel_chunk_t *chunk)
{
    g_voxel_chunks->gpu_queue.push_material(&chunk->push_k, sizeof(chunk->push_k), &chunk->gpu_mesh);
}


#include "ttable.inc"


internal_function inline float32_t lerp(float32_t a, float32_t b, float32_t x)
{
    return((x - a) / (b - a));
}


internal_function inline vector3_t interpolate(const vector3_t &a, const vector3_t &b, float32_t x)
{
    return(a + x * (b - a));
}


internal_function inline void push_vertex_to_triangle_array(uint8_t v0, uint8_t v1, vector3_t *vertices, voxel_chunk_t *chunk, uint8_t *voxel_values, uint8_t surface_level)
{
    float32_t surface_level_f = (float32_t)surface_level;
    float32_t voxel_value0 = (float32_t)voxel_values[v0];
    float32_t voxel_value1 = (float32_t)voxel_values[v1];

    if (voxel_value0 > voxel_value1)
    {
        float32_t tmp = voxel_value0;
        voxel_value0 = voxel_value1;
        voxel_value1 = tmp;

        uint8_t tmp_v = v0;
        v0 = v1;
        v1 = tmp_v;
    }

    float32_t interpolated_voxel_values = lerp(voxel_value0, voxel_value1, surface_level_f);
    
    vector3_t vertex = interpolate(vertices[v0], vertices[v1], interpolated_voxel_values);
    
    chunk->mesh_vertices[chunk->vertex_count++] = vertex;
}


internal_function void update_chunk_mesh_struct_vertex_count(voxel_chunk_t *chunk)
{
    chunk->gpu_mesh.indexed_data.index_count = chunk->vertex_count;
}


global_var constexpr vector3_t NORMALIZED_CUBE_VERTICES[8] = { vector3_t(-0.5f, -0.5f, -0.5f),
                                                               vector3_t(+0.5f, -0.5f, -0.5f),
                                                               vector3_t(+0.5f, -0.5f, +0.5f),
                                                               vector3_t(-0.5f, -0.5f, +0.5f),
                                                               vector3_t(-0.5f, +0.5f, -0.5f),
                                                               vector3_t(+0.5f, +0.5f, -0.5f),
                                                               vector3_t(+0.5f, +0.5f, +0.5f),
                                                               vector3_t(-0.5f, +0.5f, +0.5f) };

global_var constexpr ivector3_t NORMALIZED_CUBE_VERTEX_INDICES[8] = { ivector3_t(0, 0, 0),
                                                                      ivector3_t(1, 0, 0),
                                                                      ivector3_t(1, 0, 1),
                                                                      ivector3_t(0, 0, 1),
                                                                      ivector3_t(0, 1, 0),
                                                                      ivector3_t(1, 1, 0),
                                                                      ivector3_t(1, 1, 1),
                                                                      ivector3_t(0, 1, 1) };


// Voxel pair is like a cube: 8 voxels
internal_function void update_chunk_mesh_voxel_pair(uint8_t *voxel_values, voxel_chunk_t *chunk, uint32_t x, uint32_t y, uint32_t z, uint8_t surface_level)
{    
    uint8_t bit_combination = 0;
    for (uint32_t i = 0; i < 8; ++i)
    {
        bool is_over_surface = (voxel_values[i] > surface_level);
        bit_combination |= is_over_surface << i;
    }

    const int8_t *triangle_entry = &TRIANGLE_TABLE[bit_combination][0];

    uint32_t edge = 0;

    int8_t edge_pair[3] = {};
                
    while(triangle_entry[edge] != -1)
    {
        int8_t edge_index = triangle_entry[edge];
        edge_pair[edge % 3] = edge_index;

        if (edge % 3 == 2)
        {
            vector3_t vertices[8] = {};
            for (uint32_t i = 0; i < 8; ++i)
            {
                vertices[i] = NORMALIZED_CUBE_VERTICES[i] + vector3_t(0.5f) + vector3_t((float32_t)x, (float32_t)y, (float32_t)z);
            }

            for (uint32_t i = 0; i < 3; ++i)
            {
                switch(edge_pair[i])
                {
                case 0: { push_vertex_to_triangle_array(0, 1, vertices, chunk, voxel_values, surface_level); } break;
                case 1: { push_vertex_to_triangle_array(1, 2, vertices, chunk, voxel_values, surface_level); } break;
                case 2: { push_vertex_to_triangle_array(2, 3, vertices, chunk, voxel_values, surface_level); } break;
                case 3: { push_vertex_to_triangle_array(3, 0, vertices, chunk, voxel_values, surface_level); } break;
                case 4: { push_vertex_to_triangle_array(4, 5, vertices, chunk, voxel_values, surface_level); } break;
                case 5: { push_vertex_to_triangle_array(5, 6, vertices, chunk, voxel_values, surface_level); } break;
                case 6: { push_vertex_to_triangle_array(6, 7, vertices, chunk, voxel_values, surface_level); } break;
                case 7: { push_vertex_to_triangle_array(7, 4, vertices, chunk, voxel_values, surface_level); } break;
                case 8: { push_vertex_to_triangle_array(0, 4, vertices, chunk, voxel_values, surface_level); } break;
                case 9: { push_vertex_to_triangle_array(1, 5, vertices, chunk, voxel_values, surface_level); } break;
                case 10: { push_vertex_to_triangle_array(2, 6, vertices, chunk, voxel_values, surface_level); } break;
                case 11: { push_vertex_to_triangle_array(3, 7, vertices, chunk, voxel_values, surface_level); } break;
                }
            }
        }

        ++edge;
    }
}


internal_function uint8_t chunk_edge_voxel_value(voxel_chunk_t *chunk, uint32_t x, uint32_t y, uint32_t z, bool *doesnt_exist)
{
    // Voxel coords
    uint32_t chunk_coord_offset_x = 0, chunk_coord_offset_y = 0, chunk_coord_offset_z = 0;
    uint32_t final_x = x, final_y = y, final_z = z;

    if (x == VOXEL_CHUNK_EDGE_LENGTH)
    {
        final_x = 0;
        chunk_coord_offset_x = 1;
    }
    if (y == VOXEL_CHUNK_EDGE_LENGTH)
    {
        final_y = 0;
        chunk_coord_offset_y = 1;
    }
    if (z == VOXEL_CHUNK_EDGE_LENGTH)
    {
        final_z = 0;
        chunk_coord_offset_z = 1;
    }

    voxel_chunk_t **chunk_ptr = get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x + chunk_coord_offset_x,
                                                                       chunk->chunk_coord.y + chunk_coord_offset_y,
                                                                       chunk->chunk_coord.z + chunk_coord_offset_z,
                                                                       g_voxel_chunks->grid_edge_size));
    *doesnt_exist = (bool)(*chunk_ptr == nullptr);
    if (*doesnt_exist)
    {
        return 0;
    }
    
    return (*chunk_ptr)->voxels[final_x][final_y][final_z];
}


void update_chunk_mesh(voxel_chunk_t *chunk, uint8_t surface_level)
{
    chunk->vertex_count = 0;

    voxel_chunk_t *x_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x + 1, chunk->chunk_coord.y, chunk->chunk_coord.z, g_voxel_chunks->grid_edge_size));
    voxel_chunk_t *y_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x, chunk->chunk_coord.y + 1, chunk->chunk_coord.z, g_voxel_chunks->grid_edge_size));
    voxel_chunk_t *z_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x, chunk->chunk_coord.y, chunk->chunk_coord.z + 1, g_voxel_chunks->grid_edge_size));
    
    voxel_chunk_t *xy_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x + 1, chunk->chunk_coord.y + 1, chunk->chunk_coord.z, g_voxel_chunks->grid_edge_size));
    voxel_chunk_t *xz_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x + 1, chunk->chunk_coord.y, chunk->chunk_coord.z + 1, g_voxel_chunks->grid_edge_size));
    voxel_chunk_t *yz_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x, chunk->chunk_coord.y + 1, chunk->chunk_coord.z + 1, g_voxel_chunks->grid_edge_size));
    voxel_chunk_t *xyz_superior = *get_voxel_chunk(convert_3d_to_1d_index(chunk->chunk_coord.x + 1, chunk->chunk_coord.y + 1, chunk->chunk_coord.z + 1, g_voxel_chunks->grid_edge_size));

    // First do the vertices that will need information from other chunks
    bool doesnt_exist = 0;
    if (x_superior)
    {
        // x_superior
        for (uint32_t z = 0; z < VOXEL_CHUNK_EDGE_LENGTH; ++z)
        {
            for (uint32_t y = 0; y < VOXEL_CHUNK_EDGE_LENGTH - 1; ++y)
            {
                doesnt_exist = 0;
                
                uint32_t x = VOXEL_CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = { chunk->voxels[x]    [y][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y, z, &doesnt_exist),//chunk->voxels[x + 1][y][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y, z + 1, &doesnt_exist),//chunk->voxels[x + 1][y][z + 1],
                                            chunk_edge_voxel_value(chunk, x,     y, z + 1, &doesnt_exist),//chunk->voxels[x]    [y][z + 1],
                     
                                            chunk->voxels[x]    [y + 1][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y + 1, z,&doesnt_exist),//chunk->voxels[x + 1][y + 1][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y + 1, z + 1, &doesnt_exist),//chunk->voxels[x + 1][y + 1][z + 1],
                                            chunk_edge_voxel_value(chunk, x,     y + 1, z + 1, &doesnt_exist) };//chunk->voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist)
                {
                    update_chunk_mesh_voxel_pair(voxel_values, chunk, x, y, z, surface_level);
                }
            }
        }
    }

    if (y_superior)
    {
        // y_superior    
        for (uint32_t z = 0; z < VOXEL_CHUNK_EDGE_LENGTH; ++z)
        {
            for (uint32_t x = 0; x < VOXEL_CHUNK_EDGE_LENGTH; ++x)
            {
                doesnt_exist = 0;
                
                uint32_t y = VOXEL_CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = { chunk->voxels[x]    [y][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y, z, &doesnt_exist),//chunk->voxels[x + 1][y][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y, z + 1, &doesnt_exist),//chunk->voxels[x + 1][y][z + 1],
                                            chunk_edge_voxel_value(chunk, x,     y, z + 1, &doesnt_exist),//chunk->voxels[x]    [y][z + 1],
                     
                                            chunk_edge_voxel_value(chunk, x, y + 1, z, &doesnt_exist),
                                            chunk_edge_voxel_value(chunk, x + 1, y + 1, z, &doesnt_exist),//chunk->voxels[x + 1][y + 1][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y + 1, z + 1, &doesnt_exist),//chunk->voxels[x + 1][y + 1][z + 1],
                                            chunk_edge_voxel_value(chunk, x,     y + 1, z + 1, &doesnt_exist) };//chunk->voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist)
                {
                    update_chunk_mesh_voxel_pair(voxel_values, chunk, x, y, z, surface_level);
                }
            }
        }
    }

    if (z_superior)
    {
        // z_superior
        for (uint32_t y = 0; y < VOXEL_CHUNK_EDGE_LENGTH - 1; ++y)
        {
            for (uint32_t x = 0; x < VOXEL_CHUNK_EDGE_LENGTH - 1; ++x)
            {
                doesnt_exist = 0;
                
                uint32_t z = VOXEL_CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = { chunk->voxels[x]    [y][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y, z, &doesnt_exist),//chunk->voxels[x + 1][y][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y, z + 1, &doesnt_exist),//chunk->voxels[x + 1][y][z + 1],
                                            chunk_edge_voxel_value(chunk, x,     y, z + 1, &doesnt_exist),//chunk->voxels[x]    [y][z + 1],
                     
                                            chunk->voxels[x]    [y + 1][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y + 1, z, &doesnt_exist),//chunk->voxels[x + 1][y + 1][z],
                                            chunk_edge_voxel_value(chunk, x + 1, y + 1, z + 1, &doesnt_exist),//chunk->voxels[x + 1][y + 1][z + 1],
                                            chunk_edge_voxel_value(chunk, x,     y + 1, z + 1, &doesnt_exist) };//chunk->voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist)
                {
                    update_chunk_mesh_voxel_pair(voxel_values, chunk, x, y, z, surface_level);
                }
            }
        }
    }
    
    for (uint32_t z = 0; z < VOXEL_CHUNK_EDGE_LENGTH - 1; ++z)
    {
        for (uint32_t y = 0; y < VOXEL_CHUNK_EDGE_LENGTH - 1; ++y)
        {
            for (uint32_t x = 0; x < VOXEL_CHUNK_EDGE_LENGTH - 1; ++x)
            {
                uint8_t voxel_values[8] = { chunk->voxels[x]    [y][z],
                                            chunk->voxels[x + 1][y][z],
                                            chunk->voxels[x + 1][y][z + 1],
                                            chunk->voxels[x]    [y][z + 1],
                     
                                            chunk->voxels[x]    [y + 1][z],
                                            chunk->voxels[x + 1][y + 1][z],
                                            chunk->voxels[x + 1][y + 1][z + 1],
                                            chunk->voxels[x]    [y + 1][z + 1] };

                update_chunk_mesh_voxel_pair(voxel_values, chunk, x, y, z, surface_level);
            }
        }
    }

    ready_chunk_for_gpu_sync(chunk);
}


void ready_chunk_for_gpu_sync(voxel_chunk_t *chunk)
{
    // If it is already scheduled for GPU sync, don't push to the update stack
    if (!chunk->should_do_gpu_sync)
    {
        g_voxel_chunks->chunks_to_gpu_sync[g_voxel_chunks->to_sync_count++] = convert_3d_to_1d_index(chunk->chunk_coord.x, chunk->chunk_coord.y, chunk->chunk_coord.z, g_voxel_chunks->grid_edge_size);
        chunk->should_do_gpu_sync = 1;
    }
}


internal_function void sync_gpu_with_chunk_state(gpu_command_queue_t *queue)
{
    for (uint32_t i = 0; i < g_voxel_chunks->to_sync_count; ++i)
    {
        voxel_chunk_t *chunk = *get_voxel_chunk((int32_t)g_voxel_chunks->chunks_to_gpu_sync[i]);

        update_chunk_mesh(chunk, 60);
        update_chunk_mesh_struct_vertex_count(chunk);
        
        update_gpu_buffer(&chunk->chunk_mesh_gpu_buffer,
                          chunk->mesh_vertices,
                          sizeof(vector3_t) * chunk->vertex_count,
                          0,
                          VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                          VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                          &queue->q);

        chunk->should_do_gpu_sync = 0;
    }

    g_voxel_chunks->to_sync_count = 0;
}


void initialize_chunk(voxel_chunk_t *chunk, vector3_t chunk_position, ivector3_t chunk_coord)
{
    chunk->xs_bottom_corner = chunk_coord * VOXEL_CHUNK_EDGE_LENGTH;
    chunk->chunk_coord = chunk_coord;
    
    memset(chunk->voxels, 0, sizeof(uint8_t) * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH);
    memset(chunk->mesh_vertices, 0, sizeof(vector3_t) * MAX_VERTICES_PER_VOXEL_CHUNK);
    
    uint32_t buffer_size = sizeof(vector3_t) * MAX_VERTICES_PER_VOXEL_CHUNK;

    make_unmappable_gpu_buffer(&chunk->chunk_mesh_gpu_buffer, buffer_size, chunk->mesh_vertices, gpu_buffer_usage_t::VERTEX_BUFFER, get_global_command_pool());

    draw_indexed_data_t indexed_data = init_draw_indexed_data_default(1, chunk->vertex_count);
    model_index_data_t model_indexed_data;
    memory_buffer_view_t<VkBuffer> buffers{1, &chunk->chunk_mesh_gpu_buffer.buffer};
    
    chunk->gpu_mesh = initialize_mesh(buffers, &indexed_data, &g_voxel_chunks->chunk_model.index_data);

    chunk->push_k.model_matrix = glm::scale(vector3_t(g_voxel_chunks->size)) * glm::translate(chunk_position);
    chunk->push_k.color = vector4_t(118.0 / 255.0, 230.0 / 255.0, 72.0 / 255.0, 1.0f);
}


voxel_chunk_t **get_voxel_chunk(int32_t index)
{
    persist_var voxel_chunk_t *nul = nullptr;
    if (index == -1) return(&nul);
    return(&g_voxel_chunks->chunks[index]);
}


internal_function void dbg_render_chunk_edges(gpu_command_queue_t *queue, uniform_group_t *transforms_ubo)
{
    graphics_pipeline_t *dbg_chunk_render_ppln = g_pipeline_manager->get(g_voxel_chunks->dbg_chunk_edge_pipeline);
    command_buffer_bind_pipeline(&dbg_chunk_render_ppln->pipeline, &queue->q);

    command_buffer_bind_descriptor_sets(&dbg_chunk_render_ppln->layout, {1, transforms_ubo}, &queue->q);

    for (uint32_t z = 0; z < g_voxel_chunks->grid_edge_size; ++z)
    {
        for (uint32_t y = 0; y < g_voxel_chunks->grid_edge_size; ++y)
        {
            for (uint32_t x = 0; x < g_voxel_chunks->grid_edge_size; ++x)
            {
                voxel_chunk_t *chunk = *get_voxel_chunk(convert_3d_to_1d_index(x, y, z, g_voxel_chunks->grid_edge_size));
                
                struct push_k_t
                {
                    alignas(16) matrix4_t model_matrix;
                    alignas(16) vector4_t positions[8];
                    alignas(16) vector4_t color;
                } pk;

                pk.model_matrix = chunk->push_k.model_matrix;

                pk.positions[0] = vector4_t(0, 0, 0, 1.0f);
                pk.positions[1] = vector4_t(0, 16, 0, 1.0f);
                pk.positions[2] = vector4_t(0, 16, 16, 1.0f);
                pk.positions[3] = vector4_t(0, 0, 16, 1.0f);

                pk.positions[4] = vector4_t(16, 0, 0, 1.0f);
                pk.positions[5] = vector4_t(16, 16, 0, 1.0f);
                pk.positions[6] = vector4_t(16, 16, 16, 1.0f);
                pk.positions[7] = vector4_t(16, 0, 16, 1.0f);

                pk.color = vector4_t(0.0f, 0.0f, 1.0f, 1.0f);

                command_buffer_push_constant(&pk, sizeof(pk), 0, VK_SHADER_STAGE_VERTEX_BIT, dbg_chunk_render_ppln->layout, &queue->q);

                command_buffer_draw(&queue->q, 24, 1, 0, 0);
            }
        }
    }
}


internal_function movement_axes_t compute_movement_axes(const vector3_t &view_direction, const vector3_t &up)
{
    vector3_t right = glm::cross(view_direction, up);
    vector3_t forward = glm::cross(up, right);
    movement_axes_t axes = {right, up, forward};
    return(axes);
}


internal_function void push_collision_vertex(uint8_t v0, uint8_t v1, vector3_t *vertices, uint8_t *voxel_values, uint8_t surface_level, vector3_t *dst_array, uint32_t *count)
{
    float32_t surface_level_f = (float32_t)surface_level;
    float32_t voxel_value0 = (float32_t)voxel_values[v0];
    float32_t voxel_value1 = (float32_t)voxel_values[v1];

    if (voxel_value0 > voxel_value1)
    {
        float32_t tmp = voxel_value0;
        voxel_value0 = voxel_value1;
        voxel_value1 = tmp;

        uint8_t tmp_v = v0;
        v0 = v1;
        v1 = tmp_v;
    }

    float32_t interpolated_voxel_values = lerp(voxel_value0, voxel_value1, surface_level_f);
    
    vector3_t vertex = interpolate(vertices[v0], vertices[v1], interpolated_voxel_values);
    
    dst_array[(*count)++] = vertex;
}


internal_function void push_collision_triangles_vertices(uint8_t *voxel_values, uint32_t x, uint32_t y, uint32_t z, uint8_t surface_level, vector3_t *dst_array, uint32_t *count, uint32_t max)
{
    uint8_t bit_combination = 0;
    for (uint32_t i = 0; i < 8; ++i)
    {
        bool is_over_surface = (voxel_values[i] > surface_level);
        bit_combination |= is_over_surface << i;
    }

    const int8_t *triangle_entry = &TRIANGLE_TABLE[bit_combination][0];

    uint32_t edge = 0;

    int8_t edge_pair[3] = {};
                
    while(triangle_entry[edge] != -1)
    {
        if (*count + 3 >= max)
        {
            break;
        }
        
        int8_t edge_index = triangle_entry[edge];
        edge_pair[edge % 3] = edge_index;

        if (edge % 3 == 2)
        {
            vector3_t vertices[8] = {};
            for (uint32_t i = 0; i < 8; ++i)
            {
                vertices[i] = NORMALIZED_CUBE_VERTICES[i] + vector3_t(0.5f) + vector3_t((float32_t)x, (float32_t)y, (float32_t)z);
            }

            for (uint32_t i = 0; i < 3; ++i)
            {
                switch(edge_pair[i])
                {
                case 0: { push_collision_vertex(0, 1, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 1: { push_collision_vertex(1, 2, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 2: { push_collision_vertex(2, 3, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 3: { push_collision_vertex(3, 0, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 4: { push_collision_vertex(4, 5, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 5: { push_collision_vertex(5, 6, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 6: { push_collision_vertex(6, 7, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 7: { push_collision_vertex(7, 4, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 8: { push_collision_vertex(0, 4, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 9: { push_collision_vertex(1, 5, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 10: { push_collision_vertex(2, 6, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 11: { push_collision_vertex(3, 7, vertices, voxel_values, surface_level, dst_array, count); } break;
                }
            }
        }

        ++edge;
    }
}


internal_function bool32_t is_point_in_triangle(const vector3_t &point, const vector3_t &tri_point_a, const vector3_t &tri_point_b, const vector3_t &tri_point_c)
{
    vector3_t cross11 = glm::cross((tri_point_c - tri_point_b), (point - tri_point_b));
    vector3_t cross12 = glm::cross((tri_point_c - tri_point_b), (tri_point_a - tri_point_b));
    float32_t d1 = glm::dot(cross11, cross12);
    if(d1 >= 0)
    {
        vector3_t cross21 = glm::cross((tri_point_c - tri_point_a), (point - tri_point_a));
        vector3_t cross22 = glm::cross((tri_point_c - tri_point_a), (tri_point_b - tri_point_a));
        float32_t d2 = glm::dot(cross21, cross22);
        if(d2 >= 0)
        {
            vector3_t cross31 = glm::cross((tri_point_b - tri_point_a), (point - tri_point_a));
            vector3_t cross32 = glm::cross((tri_point_b - tri_point_a), (tri_point_c - tri_point_a));
            float32_t d3 = glm::dot(cross31, cross32);
            if(d3 >= 0)
            {
                return 1;
            }
        }
    }
    return 0;
}


// This function solves the quadratic eqation "At^2 + Bt + C = 0" and is found in Kasper Fauerby's paper on collision detection and response
internal_function bool get_smallest_root(float32_t a, float32_t b, float32_t c, float32_t max_r, float32_t *root) 
{
    // Check if a solution exists
    float determinant = b * b - 4.0f * a *c;
    // If determinant is negative it means no solutions.
    if (determinant < 0.0f) return false;
    // calculate the two roots: (if determinant == 0 then
    // x1==x2 but lets disregard that slight optimization)
    float sqrt_d = sqrt(determinant);
    float r1 = (-b - sqrt_d) / (2 * a);
    float r2 = (-b + sqrt_d) / (2 * a);
    // Sort so x1 <= x2
    if (r1 > r2)
    {
        float32_t temp = r2;
        r2 = r1;
        r1 = temp;
    }
    // Get lowest root:
    if (r1 > 0 && r1 < max_r)
    {
        *root = r1;
        return true;
    }
    // It is possible that we want x2 - this can happen
    // if x1 < 0
    if (r2 > 0 && r2 < max_r)
    {
        *root = r2;
        return true;
    }

    // No (valid) solutions
    return false;
}


internal_function float32_t get_plane_constant(const vector3_t &plane_point, const vector3_t &plane_normal)
{
    return -( (plane_point.x * plane_normal.x) + (plane_point.y * plane_normal.y) + (plane_point.z * plane_normal.z) );
}


internal_function float32_t squared(float32_t f)
{
    return(f * f);
}


internal_function float32_t distance_squared(const vector3_t &dir)
{
    return glm::dot(dir, dir);
}


internal_function void check_collision_with_vertex(const vector3_t &es_sphere_velocity, const vector3_t &es_sphere_position, const vector3_t &es_vertex, const vector3_t &es_surface_normal, collision_t *collision)
{
    float32_t a = distance_squared(es_sphere_velocity);
    float32_t b = 2.0f * glm::dot(es_sphere_velocity, es_sphere_position - es_vertex);
    float32_t c = distance_squared(es_vertex - es_sphere_position) - 1.0f;
            
    float32_t new_resting_instance;
    if (get_smallest_root(a, b, c, 1.0f, &new_resting_instance))
    {
        // TODO: Make sure that we are not using glm::length
        float32_t es_distance = glm::length(new_resting_instance * es_sphere_velocity);
                
        if (es_distance < collision->es_distance)
        {
            collision->detected = 1;
            collision->primitive_type = collision_primitive_type_t::CPT_VERTEX;
            collision->es_distance = es_distance;
            collision->es_contact_point = es_vertex;
            collision->es_normal = es_surface_normal;
        }
    }
}


internal_function void check_collision_with_edge(const vector3_t &es_sphere_velocity, const vector3_t &es_sphere_position, const vector3_t &es_vertex_a, const vector3_t &es_vertex_b, const vector3_t &es_surface_normal, collision_t *collision)
{
    vector3_t es_edge_diff = es_vertex_b - es_vertex_a;
    vector3_t es_sphere_pos_to_vertex = es_vertex_a - es_sphere_position;
        
    float32_t a = distance_squared(es_edge_diff) * -distance_squared(es_sphere_velocity) + squared(glm::dot(es_edge_diff, es_sphere_velocity));
    float32_t b = distance_squared(es_edge_diff) * 2.0f * glm::dot(es_sphere_velocity, es_sphere_pos_to_vertex) - 2.0f * (glm::dot(es_edge_diff, es_sphere_velocity) * glm::dot(es_edge_diff, es_sphere_pos_to_vertex));
    float32_t c = distance_squared(es_edge_diff) * (1.0f - distance_squared(es_sphere_pos_to_vertex)) + squared(glm::dot(es_edge_diff, es_sphere_pos_to_vertex));

    float32_t new_resting_instance;
    if (get_smallest_root(a, b, c, 1.0f, &new_resting_instance))
    {
        float32_t in_edge_proportion = (glm::dot(es_edge_diff, es_sphere_velocity) * new_resting_instance - glm::dot(es_edge_diff, es_sphere_pos_to_vertex)) / distance_squared(es_edge_diff);
            
        if (in_edge_proportion >= 0.0f && in_edge_proportion <= 1.0f)
        {
            vector3_t es_sphere_contact_point = es_vertex_a + in_edge_proportion * es_edge_diff;
            // TODO: Get rid of glm::length
            float32_t es_distance = glm::length(new_resting_instance * es_sphere_velocity);
            
            if (es_distance < collision->es_distance)
            {
                collision->detected = 1;
                collision->primitive_type = collision_primitive_type_t::CPT_EDGE;
                collision->es_distance = es_distance;
                collision->es_contact_point = es_sphere_contact_point;
                collision->es_normal = es_surface_normal;
            }
        }
    }
}


internal_function void collide_with_triangle(vector3_t *triangle_vertices, const vector3_t &es_center, const vector3_t &es_velocity, collision_t *closest)
{
    bool found_collision = 0;
    float32_t first_resting_instance;
    
    vector3_t es_fa = triangle_vertices[0];
    vector3_t es_fb = triangle_vertices[1];
    vector3_t es_fc = triangle_vertices[2];

    vector3_t es_up_normal_of_triangle = glm::normalize(glm::cross(es_fb - es_fa, es_fc - es_fa));
    
    float32_t velocity_dot_normal = glm::dot(glm::normalize(es_velocity), es_up_normal_of_triangle);
    
    if (velocity_dot_normal > 0.0f)
    {
        return;
    }

    float32_t plane_constant = -( (es_fa.x * es_up_normal_of_triangle.x) + (es_fa.y * es_up_normal_of_triangle.y) + (es_fa.z * es_up_normal_of_triangle.z) );

    bool32_t must_check_only_for_edges_and_vertices = 0;
    float32_t normal_dot_velocity = glm::dot(es_velocity, es_up_normal_of_triangle);
    float32_t sphere_plane_distance = glm::dot(es_center, es_up_normal_of_triangle) + plane_constant;
    
    if (normal_dot_velocity == 0.0f)
    {
        if (glm::abs(sphere_plane_distance) >= 1.0f)
        {
            return;
        }
        else
        {
            must_check_only_for_edges_and_vertices = 1;
        }
    }

    if (!must_check_only_for_edges_and_vertices)
    {
        // Check collision with triangle face
        first_resting_instance = (1.0f - sphere_plane_distance) / normal_dot_velocity;
        float32_t second_resting_instance = (-1.0f - sphere_plane_distance) / normal_dot_velocity;

        if (first_resting_instance > second_resting_instance)
        {
            float32_t f = first_resting_instance;
            first_resting_instance = second_resting_instance;
            second_resting_instance = f;
        }
        if (first_resting_instance > 1.0f || second_resting_instance < 0.0f)
        {
            return;
        }
        if (first_resting_instance < 0.0f) first_resting_instance = 0.0f;
        if (second_resting_instance < 1.0f) second_resting_instance = 1.0f;

        vector3_t es_contact_point = es_center + (first_resting_instance * es_velocity) - es_up_normal_of_triangle;

        if (is_point_in_triangle(es_contact_point, es_fa, es_fb, es_fc))
        {
            // TODO: Get rid of glm::length
            float32_t es_distance = glm::length(es_velocity * first_resting_instance);
            if (es_distance < closest->es_distance)
            {
                float32_t sphere_point_plane_distance = glm::dot(es_center - es_up_normal_of_triangle, es_up_normal_of_triangle) + plane_constant;
                if (sphere_point_plane_distance < 0.0f && !closest->under_terrain)
                {
                    // Adjust the sphere position, and call the function
                    vector3_t es_new_sphere_position = es_center - es_up_normal_of_triangle * sphere_point_plane_distance;
                    
                    closest->under_terrain = 1;
                    closest->es_at = es_new_sphere_position;
                    closest->es_normal = es_up_normal_of_triangle;

                    collide_with_triangle(triangle_vertices, es_new_sphere_position, es_velocity, closest);
                    
                    return;
                }
                
                found_collision = 1;
                closest->detected = 1;
                closest->primitive_type = collision_primitive_type_t::CPT_FACE;
                closest->es_distance = es_distance;
                closest->es_contact_point = es_contact_point;
                closest->es_normal = es_up_normal_of_triangle;
            }
        }
    }
    
    if (!found_collision)
    {
        // Check collision with vertices
        check_collision_with_vertex(es_velocity, es_center, es_fa, es_up_normal_of_triangle, closest);
        check_collision_with_vertex(es_velocity, es_center, es_fb, es_up_normal_of_triangle, closest);
        check_collision_with_vertex(es_velocity, es_center, es_fc, es_up_normal_of_triangle, closest);

        // Check collision with edges
        check_collision_with_edge(es_velocity, es_center, es_fa, es_fb, es_up_normal_of_triangle, closest);
        check_collision_with_edge(es_velocity, es_center, es_fb, es_fc, es_up_normal_of_triangle, closest);
        check_collision_with_edge(es_velocity, es_center, es_fc, es_fa, es_up_normal_of_triangle, closest);
    }
}


internal_function collision_t collide(const vector3_t &ws_center, const vector3_t &ws_size, const vector3_t &ws_velocity, uint32_t recurse_depth, collision_t previous_collision)
{
    vector3_t es_center = ws_center / ws_size;
    vector3_t es_velocity = ws_velocity / ws_size;
    
    ivector3_t xs_cube_range = ivector3_t(glm::ceil(ws_to_xs(ws_center + ws_size)));
    ivector3_t xs_cube_min = ivector3_t(glm::floor(ws_to_xs(ws_center - ws_size)));
    xs_cube_range = xs_cube_range - xs_cube_min;

    bool is_between_chunks = 0;
    ivector3_t min_voxel_coord = get_voxel_coord(xs_cube_min);
    ivector3_t max_voxel_coord = get_voxel_coord(xs_cube_min + xs_cube_range);
    // If this is true, then the player's collision box will be between chunks
    if (max_voxel_coord.x < min_voxel_coord.x ||
        max_voxel_coord.y < min_voxel_coord.y ||
        max_voxel_coord.z < min_voxel_coord.z ||
        max_voxel_coord.x == 15 ||
        max_voxel_coord.y == 15 ||
        max_voxel_coord.z == 15)
    {
        is_between_chunks = 1;
    }

    uint32_t collision_vertex_count = 0;
    uint32_t max_vertices = 3 * 5 * glm::dot(vector3_t(xs_cube_range), vector3_t(xs_cube_range)) / 2;
    vector3_t *triangle_vertices = (vector3_t *)allocate_linear(sizeof(vector3_t) * max_vertices);
    
    for (int32_t z = xs_cube_min.z; z < xs_cube_min.z + xs_cube_range.z; ++z)
    {
        for (int32_t y = xs_cube_min.y; y < xs_cube_min.y + xs_cube_range.y; ++y)
        {
            for (int32_t x = xs_cube_min.x; x < xs_cube_min.x + xs_cube_range.x; ++x)
            {
                ivector3_t voxel_pair_origin = ivector3_t(x, y, z);
                voxel_chunk_t *chunk = get_chunk_encompassing_point(voxel_pair_origin);

                bool doesnt_exist = 0;

                uint8_t voxel_values[8] = {};

                ivector3_t cs_coord = get_voxel_coord(ivector3_t(x, y, z));
                
                if (is_between_chunks)
                {
                    voxel_values[0] = chunk->voxels[cs_coord.x]    [cs_coord.y][cs_coord.z];
                    voxel_values[1] = chunk_edge_voxel_value(chunk, cs_coord.x + 1, cs_coord.y, cs_coord.z, &doesnt_exist);
                    voxel_values[2] = chunk_edge_voxel_value(chunk, cs_coord.x + 1, cs_coord.y, cs_coord.z + 1, &doesnt_exist);
                    voxel_values[3] = chunk_edge_voxel_value(chunk, cs_coord.x,     cs_coord.y, cs_coord.z + 1, &doesnt_exist);
                    
                    voxel_values[4] = chunk_edge_voxel_value(chunk, cs_coord.x,     cs_coord.y + 1, cs_coord.z, &doesnt_exist);
                    voxel_values[5] = chunk_edge_voxel_value(chunk, cs_coord.x + 1, cs_coord.y + 1, cs_coord.z, &doesnt_exist);
                    voxel_values[6] = chunk_edge_voxel_value(chunk, cs_coord.x + 1, cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist);
                    voxel_values[7] = chunk_edge_voxel_value(chunk, cs_coord.x,     cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist);
                }
                else
                {
                    voxel_values[0] = chunk->voxels[cs_coord.x]    [cs_coord.y][cs_coord.z];
                    voxel_values[1] = chunk->voxels[cs_coord.x + 1][cs_coord.y][cs_coord.z];
                    voxel_values[2] = chunk->voxels[cs_coord.x + 1][cs_coord.y][cs_coord.z + 1];
                    voxel_values[3] = chunk->voxels[cs_coord.x]    [cs_coord.y][cs_coord.z + 1];
                    
                    voxel_values[4] = chunk->voxels[cs_coord.x]    [cs_coord.y + 1][cs_coord.z];
                    voxel_values[5] = chunk->voxels[cs_coord.x + 1][cs_coord.y + 1][cs_coord.z];
                    voxel_values[6] = chunk->voxels[cs_coord.x + 1][cs_coord.y + 1][cs_coord.z + 1];
                    voxel_values[7] = chunk->voxels[cs_coord.x]    [cs_coord.y + 1][cs_coord.z + 1];
                }

                push_collision_triangles_vertices(voxel_values, x, y, z, 60, triangle_vertices, &collision_vertex_count, max_vertices);
            }
        }
    }

    collision_t closest_collision = {};
    closest_collision.es_distance = 1000.0f;
    
    for (uint32_t triangle = 0; triangle < collision_vertex_count / 3; ++triangle)
    {
        vector3_t *triangle_ptr = &triangle_vertices[triangle * 3];

        // Convert from xs to es (ellipsoid space)
        for (uint32_t i = 0; i < 3; ++i)
        {
            // Converts to world space
            triangle_ptr[i] = triangle_ptr[i] - vector3_t((float32_t)g_voxel_chunks->grid_edge_size / 2.0f) * (float32_t)(VOXEL_CHUNK_EDGE_LENGTH);
            triangle_ptr[i] *= g_voxel_chunks->size;
            // Converts to ellipsoid space
            triangle_ptr[i] /= ws_size;
        }

        collide_with_triangle(triangle_ptr, es_center, es_velocity, &closest_collision);
    }

    const float32_t es_very_close_distance_from_terrain = .0f;
    
    if (closest_collision.detected)
    {
        uint32_t max_recursion_depth = 5;

        // TODO: Do not calculate the ellipsoid space of these values again, just do it once at the beginning of the function
        vector3_t es_sphere_position = es_center;
        vector3_t es_sphere_velocity = es_velocity;

        vector3_t es_new_sphere_position = es_sphere_position;
        vector3_t es_sphere_destination_point = es_sphere_position + es_sphere_velocity;
            
        if (closest_collision.es_distance >= es_very_close_distance_from_terrain)
        {
            vector3_t es_normalized_velocity = glm::normalize(es_sphere_velocity);
            vector3_t es_scaled_velocity = es_normalized_velocity * (closest_collision.es_distance - es_very_close_distance_from_terrain);
            es_new_sphere_position = es_sphere_position + es_scaled_velocity;

            closest_collision.es_contact_point -= es_very_close_distance_from_terrain * es_normalized_velocity;
        }

        // Get slide plane information
        vector3_t es_slide_plane_point = closest_collision.es_contact_point;
        vector3_t es_slide_plane_normal = glm::normalize(es_new_sphere_position - closest_collision.es_contact_point);

        float32_t plane_constant = get_plane_constant(es_slide_plane_point, es_slide_plane_normal);
        float32_t dest_point_dist_from_plane = glm::dot(es_sphere_destination_point, es_slide_plane_normal) + plane_constant;

        vector3_t es_new_sphere_destination_point = es_sphere_destination_point - dest_point_dist_from_plane * es_slide_plane_normal;
        vector3_t es_new_velocity = es_new_sphere_destination_point - closest_collision.es_contact_point;

        float32_t new_velocity_distance_squared = distance_squared(es_new_velocity);
        float32_t very_close_distance_squared = squared(es_very_close_distance_from_terrain);

        if (new_velocity_distance_squared < very_close_distance_squared)
        {
            collision_t ret = {};
            ret.detected = 1;
            ret.es_at = es_new_sphere_position;
            ret.es_velocity = es_new_velocity;
            ret.es_normal = es_slide_plane_normal;
            return(ret);
        }
        // There was a collision, must recurse
        else if (recurse_depth < max_recursion_depth/* && slide*/)
        {
            collision_t current_collision = {};
            current_collision.detected = 1;
            current_collision.es_at = es_new_sphere_position;
            current_collision.es_velocity = es_new_velocity;
            current_collision.es_normal = es_slide_plane_normal;

            return collide(es_new_sphere_position * ws_size, ws_size, es_new_velocity * ws_size, recurse_depth + 1, current_collision);
        }
        else
        {
            collision_t ret = {};
            ret.detected = 1;
            ret.es_at = es_new_sphere_position;
            ret.es_velocity = es_new_velocity;
            ret.es_normal = es_slide_plane_normal;
            return(ret);
        }
    }
    else
    {
        collision_t ret = {};
        if (recurse_depth > 0)
        {
            ret.detected = 1;
            ret.is_currently_in_air = 1;
        }
        else
        {
            ret.detected = 0;
            ret.is_currently_in_air = 1;
        }
        ret.es_at = (ws_center + ws_velocity) / ws_size;
        ret.es_velocity = ws_velocity / ws_size;
        ret.es_normal = previous_collision.es_normal;
        return(ret);
    }
    
    return {};
}


// ********************* Player code ***************************

internal_function player_t *get_main_player(void)
{
    if (g_entities->main_player == -1)
    {
        return nullptr;
    }
    else
    {
        return &g_entities->player_list[g_entities->main_player];
    }
}


internal_function void push_player_to_queue(player_t *e_ptr, mesh_t *mesh, gpu_material_submission_queue_t *queue)
{
    rendering_component_t *rendering = &e_ptr->rendering;
    animation_component_t *animation = &e_ptr->animation;

    uniform_group_t *group = group = &animation->animation_instance.group;
    
    queue->push_material(&rendering->push_k,
			 sizeof(rendering->push_k),
                         mesh,
                         group);
}


internal_function void push_player_to_animated_queue(player_t *e)
{
    push_player_to_queue(e, &g_entities->player_mesh, &g_entities->player_submission_queue);
}


internal_function void push_player_to_rolling_queue(player_t *e)
{
    rendering_component_t *component = &e->rendering;

    uniform_group_t *group = nullptr;
    
    g_entities->rolling_player_submission_queue.push_material(&component->push_k,
                                                                  sizeof(component->push_k),
                                                                  &g_entities->rolling_player_mesh,
                                                                  group);
}


internal_function player_t *get_player(const constant_string_t &name)
{
    player_handle_t v = *g_entities->name_map.get(name.hash);
    return(&g_entities->player_list[v]);
}


player_t *get_player(player_handle_t v)
{
    return(&g_entities->player_list[v]);
}


// TODO: Implement camera orientation interpolation when changing surface normals
internal_function void update_camera_component(camera_component_t *camera_component, player_t *player, float32_t dt)
{
    camera_t *camera = get_camera(camera_component->camera);

    vector3_t up = vector3_t(0, 1, 0);
        
    vector3_t camera_position = player->ws_p + vector3_t(0.0f, player->size.x, 0.0f);
    if (camera_component->is_third_person)
    {
        vector3_t right = glm::cross(player->ws_d, vector3_t(0.0f, 1.0f, 0.0f));
        camera_position += right * player->size.x + -camera_component->distance_from_player * player->ws_d;
    }
        
    camera->v_m = glm::lookAt(camera_position, player->ws_p + vector3_t(0.0f, 1.0f, 0.0f) + player->ws_d, up);

    // TODO: Don't need to calculate this every frame, just when parameters change
    camera->compute_projection();

    camera->p = camera_position;
    camera->d = player->ws_d;
    camera->u = up;
}


internal_function void update_animation_component(animation_component_t *animation, player_t *player, float32_t dt)
{
    player_t::animated_state_t previous_state = player->animated_state;
    player_t::animated_state_t new_state;
        
    uint32_t moving = 0;
        
    if (player->action_flags & (1 << action_flags_t::ACTION_FORWARD))
    {
        if (player->action_flags & (1 << action_flags_t::ACTION_RUN))
        {
            new_state = player_t::animated_state_t::RUN; moving = 1;
        }
        else
        {
            new_state = player_t::animated_state_t::WALK; moving = 1;
        }
    }
    if (player->action_flags & (1 << action_flags_t::ACTION_LEFT)); 
    if (player->action_flags & (1 << action_flags_t::ACTION_DOWN));
    if (player->action_flags & (1 << action_flags_t::ACTION_RIGHT));
        
    if (!moving)
    {
        new_state = player_t::animated_state_t::IDLE;
    }

    if (player->is_sitting)
    {
        new_state = player_t::animated_state_t::SITTING;
    }

    if (player->is_in_air)
    {
        new_state = player_t::animated_state_t::HOVER;
    }

    if (player->is_sliding_not_rolling_mode)
    {
        new_state = player_t::animated_state_t::SLIDING_NOT_ROLLING_MODE;
    }

    if (new_state != previous_state)
    {
        player->animated_state = new_state;
        switch_to_cycle(&animation->animation_instance, new_state);
    }
        
    interpolate_skeleton_joints_into_instance(dt, &animation->animation_instance);
}


internal_function void update_animation_gpu_data(gpu_command_queue_t *queue)
{
    for (uint32_t i = 0; i < g_entities->player_count; ++i)
    {
        player_t *player = &g_entities->player_list[i];
        struct animation_component_t *animation = &player->animation;

        update_animated_instance_ubo(queue, &animation->animation_instance);
    }
}


internal_function void update_rendering_component(rendering_component_t *rendering, player_t *player, float32_t dt)
{
    persist_var const matrix4_t CORRECTION_90 = glm::rotate(glm::radians(90.0f), vector3_t(0.0f, 1.0f, 0.0f));

    vector3_t view_dir = glm::normalize(player->ws_d);
    float32_t dir_x = view_dir.x;
    float32_t dir_z = view_dir.z;
    float32_t rotation_angle = atan2(dir_z, dir_x);

    matrix4_t rot_matrix = glm::rotate(-rotation_angle, vector3_t(0.0f, 1.0f, 0.0f));
        
    if (rendering->enabled)
    {
        rendering->push_k.ws_t = glm::translate(player->ws_p) * CORRECTION_90 * rot_matrix * player->rolling_rotation * glm::scale(player->size);
    }
    else
    {
        rendering->push_k.ws_t = matrix4_t(0.0f);
    }

    if (player->rolling_mode)
    {
        push_player_to_rolling_queue(player);
    }
    else
    {
        push_player_to_animated_queue(player);
    }
}


internal_function void update_terraform_power_component(terraform_power_component_t *terraform_power, player_t *player, float32_t dt)
{
    uint32_t *action_flags = &player->action_flags;

    if (*action_flags & (1 << action_flags_t::ACTION_TERRAFORM_DESTROY))
    {
        ray_cast_terraform(player->ws_p, player->ws_d, 70.0f, dt, 60, 1);
    }

    if (*action_flags & (1 << action_flags_t::ACTION_TERRAFORM_ADD))
    {
        ray_cast_terraform(player->ws_p, player->ws_d, 70.0f, dt, 60, 0);
    }
}


internal_function void update_standing_player_physics(physics_component_t *component, player_t *e, uint32_t *action_flags, float32_t dt)
{
}

internal_function void update_rolling_player_physics(struct physics_component_t *component, player_t *player, uint32_t *action_flags, float32_t dt)
{
    // Only happens at the beginning of the game
    if (player->is_entering)
    {
        player->entering_acceleration += dt * 2.0f;
        // Go in the direction that the player is facing
        player->ws_v = player->entering_acceleration * player->ws_d;
    }
    else
    {
        if (component->state == entity_physics_state_t::IN_AIR)
        {
            player->ws_v += -player->ws_up * 9.81f * dt;
        }
        else if (component->state == entity_physics_state_t::ON_GROUND)
        {
            movement_axes_t axes = compute_movement_axes(player->ws_d, player->ws_up);

            component->axes = vector3_t(0);
            if (player->action_flags & (1 << action_flags_t::ACTION_FORWARD))
            {
                component->axes.z += component->acceleration;
            }
            if (player->action_flags & (1 << action_flags_t::ACTION_LEFT))
            {
                component->axes.x -= component->acceleration;
            }
            if (player->action_flags & (1 << action_flags_t::ACTION_BACK))
            {
                component->axes.z -= component->acceleration;
            }
            if (player->action_flags & (1 << action_flags_t::ACTION_RIGHT))
            {
                component->axes.x += component->acceleration;
            }

            vector3_t result_acceleration_vector = component->axes.x * axes.right + component->axes.y * axes.up + component->axes.z * axes.forward;

            player->ws_v += result_acceleration_vector * dt * 10.0f;
            player->ws_v -= player->ws_up * 9.81f * dt;

            // Friction
            persist_var constexpr float32_t TERRAIN_ROUGHNESS = .5f;
            float32_t cos_theta = glm::dot(-player->ws_up, -player->ws_up);
            vector3_t friction = -player->ws_v * TERRAIN_ROUGHNESS * 9.81f * .5f;
            player->ws_v += friction * dt;
        }
    }

    collision_t collision = collide(player->ws_p, player->size, player->ws_v * dt, 0, {});
    if (collision.detected)
    {
        if (player->is_entering)
        {
            player->is_entering = 0;
        }

        if (component->state == entity_physics_state_t::IN_AIR)
        {
            movement_axes_t axes = compute_movement_axes(player->ws_d, player->ws_up);
            player->ws_v = glm::normalize(glm::proj(player->ws_v, axes.forward));
        }

        player->ws_p = collision.es_at * player->size;
        
        // If there "was" a collision (may not be on the ground right now as might have "slid" off) player's gravity pull direction changed
        player->ws_up = glm::normalize((collision.es_normal * player->size));

        component->state = entity_physics_state_t::ON_GROUND;
    }
    else
    {
        // If there was no collision, update position (velocity should be the same)
        player->ws_p = collision.es_at * player->size;
        player->ws_v = (collision.es_velocity * player->size) / dt;

        component->state = entity_physics_state_t::IN_AIR;
    }

    /*if (collision.is_currently_in_air)
    {
        component->state = entity_physics_state_t::IN_AIR;
    }
    else
    {
        component->state = entity_physics_state_t::ON_GROUND;
        }*/
}


internal_function void update_not_physically_affected_player(struct physics_component_t *component, player_t *e, uint32_t *action_flags, float32_t dt)
{
    vector3_t result_force = vector3_t(0.0f);

    vector3_t right = glm::normalize(glm::cross(e->ws_d, vector3_t(0.0f, 1.0f, 0.0f)));
    vector3_t forward = glm::normalize(glm::cross(vector3_t(0.0f, 1.0f, 0.0f), right));

    if (*action_flags & (1 << action_flags_t::ACTION_FORWARD)) result_force += forward;
    if (*action_flags & (1 << action_flags_t::ACTION_BACK)) result_force -= forward;
    if (*action_flags & (1 << action_flags_t::ACTION_RIGHT)) result_force += right;
    if (*action_flags & (1 << action_flags_t::ACTION_LEFT)) result_force -= right;
    if (*action_flags & (1 << action_flags_t::ACTION_UP)) result_force += vector3_t(0.0f, 1.0f, 0.0f);
    if (*action_flags & (1 << action_flags_t::ACTION_DOWN)) result_force -= vector3_t(0.0f, 1.0f, 0.0f);

    result_force *= 20.0f * e->size.x;
    
    collision_t collision = collide(e->ws_p, e->size, result_force * dt, 0, {});
    e->ws_p = collision.es_at * e->size;
}


internal_function void update_physics_component(physics_component_t *physics, player_t *player, float32_t dt)
{
    uint32_t *action_flags = &player->action_flags;
        
    if (physics->enabled)
    {
        if (player->rolling_mode)
        {
            update_rolling_player_physics(physics, player, action_flags, dt);
        }
        else
        {
            update_standing_player_physics(physics, player, action_flags, dt);
        }
    }
    else
    {
        update_not_physically_affected_player(physics, player, action_flags, dt);
    }
}


internal_function player_handle_t add_player(const player_t &e)
{
    player_handle_t view;
    view = g_entities->player_count;

    g_entities->name_map.insert(e.id.hash, view);
    
    g_entities->player_list[g_entities->player_count++] = e;

    auto e_ptr = get_player(view);
    e_ptr->index = view;

    return(view);
}


internal_function void update_entities(float32_t dt, application_type_t app_type)
{
    for (uint32_t player_index = 0; player_index < g_entities->player_count; ++player_index)
    {
        player_t *player = &g_entities->player_list[player_index];
        
        switch (app_type)
        {
        case application_type_t::WINDOW_APPLICATION_MODE:
            {
                update_physics_component(&player->physics, player, dt);
                update_camera_component(&player->camera, player, dt);
                update_rendering_component(&player->rendering, player, dt);
                update_animation_component(&player->animation, player, dt);
                update_terraform_power_component(&player->terraform_power, player, dt);
            } break;
        case application_type_t::CONSOLE_APPLICATION_MODE:
            {
                update_physics_component(&player->physics, player, dt);
            } break;
        }
    }    
}


void make_player_main(player_handle_t player_handle)
{
    g_entities->main_player = player_handle;
}


internal_function void initialize_entities_graphics_data(VkCommandPool *cmdpool, input_state_t *input_state)
{
    g_entities->rolling_player_mesh = load_mesh(mesh_file_format_t::CUSTOM_MESH, "models/icosphere.mesh_custom", cmdpool);
    g_entities->rolling_player_model = make_mesh_attribute_and_binding_information(&g_entities->rolling_player_mesh);
    g_entities->rolling_player_model.index_data = g_entities->rolling_player_mesh.index_data;
    
    g_entities->player_mesh = load_mesh(mesh_file_format_t::CUSTOM_MESH, "models/spaceman.mesh_custom", cmdpool);
    g_entities->player_model = make_mesh_attribute_and_binding_information(&g_entities->player_mesh);
    g_entities->player_model.index_data = g_entities->player_mesh.index_data;
    g_entities->player_mesh_skeleton = load_skeleton("models/spaceman_walk.skeleton_custom");
    g_entities->player_mesh_cycles = load_animations("models/spaceman.animations_custom");

    uniform_layout_handle_t animation_layout_hdl = g_uniform_layout_manager->add("uniform_layout.joint_ubo"_hash);
    uniform_layout_t *animation_layout_ptr = g_uniform_layout_manager->get(animation_layout_hdl);
    uniform_layout_info_t animation_ubo_info = {};
    animation_ubo_info.push(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    *animation_layout_ptr = make_uniform_layout(&animation_ubo_info);
    
    g_entities->player_ppln = g_pipeline_manager->add("pipeline.model"_hash);
    auto *player_ppln = g_pipeline_manager->get(g_entities->player_ppln);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        render_pass_handle_t dfr_render_pass = g_render_pass_manager->get_handle("render_pass.deferred_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/lp_notex_animated.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                               shader_module_info_t{"shaders/SPV/lp_notex_animated.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
                               shader_module_info_t{"shaders/SPV/lp_notex_animated.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash),
                                         g_uniform_layout_manager->get_handle("descriptor_set_layout.2D_sampler_layout"_hash),
                                         animation_layout_hdl);
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT };
        shader_blend_states_t blending(false, false, false, false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                    VK_CULL_MODE_NONE, layouts, push_k, get_backbuffer_resolution(), blending, &g_entities->player_model,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(dfr_render_pass), 0, info);
        player_ppln->info = info;
        make_graphics_pipeline(player_ppln);
    }
    // TODO: Rename all the pipelines correctly : animated / normal
    g_entities->rolling_player_ppln = g_pipeline_manager->add("pipeline.ball"_hash);
    auto *rolling_player_ppln = g_pipeline_manager->get(g_entities->rolling_player_ppln);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        render_pass_handle_t dfr_render_pass = g_render_pass_manager->get_handle("render_pass.deferred_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/lp_notex_model.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/lp_notex_model.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
                                 shader_module_info_t{"shaders/SPV/lp_notex_model.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash),
                                         g_uniform_layout_manager->get_handle("descriptor_set_layout.2D_sampler_layout"_hash));
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT };
        shader_blend_states_t blending(false, false, false, false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                    VK_CULL_MODE_NONE, layouts, push_k, get_backbuffer_resolution(), blending, &g_entities->rolling_player_model,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(dfr_render_pass), 0, info);
        rolling_player_ppln->info = info;
        make_graphics_pipeline(rolling_player_ppln);
    }

    g_entities->dbg_hitbox_ppln = g_pipeline_manager->add("pipeline.hitboxes"_hash);
    auto *hitbox_ppln = g_pipeline_manager->get(g_entities->dbg_hitbox_ppln);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        render_pass_handle_t dfr_render_pass = g_render_pass_manager->get_handle("render_pass.deferred_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/hitbox_render.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/hitbox_render.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash));
        shader_pk_data_t push_k = {240, 0, VK_SHADER_STAGE_VERTEX_BIT};
        shader_blend_states_t blending(false, false, false, false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_LINE,
                                    VK_CULL_MODE_NONE, layouts, push_k, get_backbuffer_resolution(), blending, nullptr,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(dfr_render_pass), 0, info);
        hitbox_ppln->info = info;
        make_graphics_pipeline(hitbox_ppln);
    }

    g_entities->player_shadow_ppln = g_pipeline_manager->add("pipeline.model_shadow"_hash);
    auto *player_shadow_ppln = g_pipeline_manager->get(g_entities->player_shadow_ppln);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        auto shadow_display = get_shadow_display();
        VkExtent2D shadow_extent {shadow_display.shadowmap_w, shadow_display.shadowmap_h};
        render_pass_handle_t shadow_render_pass = g_render_pass_manager->get_handle("render_pass.shadow_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/lp_notex_model_shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                               shader_module_info_t{"shaders/SPV/lp_notex_model_shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash),
                                         animation_layout_hdl);
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT};
        shader_blend_states_t blending(false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_DEPTH_BIAS, VK_DYNAMIC_STATE_VIEWPORT);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                               VK_CULL_MODE_NONE, layouts, push_k, shadow_extent, blending, &g_entities->player_model,
                               true, 0.0f, dynamic, g_render_pass_manager->get(shadow_render_pass), 0, info);
        player_shadow_ppln->info = info;
        make_graphics_pipeline(player_shadow_ppln);
    }

    g_entities->rolling_player_shadow_ppln = g_pipeline_manager->add("pipeline.ball_shadow"_hash);
    auto *rolling_player_shadow_ppln = g_pipeline_manager->get(g_entities->rolling_player_shadow_ppln);
    {
        graphics_pipeline_info_t *info = (graphics_pipeline_info_t *)allocate_free_list(sizeof(graphics_pipeline_info_t));
        auto shadow_display = get_shadow_display();
        VkExtent2D shadow_extent {shadow_display.shadowmap_w, shadow_display.shadowmap_h};
        render_pass_handle_t shadow_render_pass = g_render_pass_manager->get_handle("render_pass.shadow_render_pass"_hash);
        shader_modules_t modules(shader_module_info_t{"shaders/SPV/model_shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
                                 shader_module_info_t{"shaders/SPV/model_shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT});
        shader_uniform_layouts_t layouts(g_uniform_layout_manager->get_handle("uniform_layout.camera_transforms_ubo"_hash));
        shader_pk_data_t push_k = {160, 0, VK_SHADER_STAGE_VERTEX_BIT};
        shader_blend_states_t blending(false);
        dynamic_states_t dynamic(VK_DYNAMIC_STATE_DEPTH_BIAS, VK_DYNAMIC_STATE_VIEWPORT);
        fill_graphics_pipeline_info(modules, VK_FALSE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL,
                                    VK_CULL_MODE_NONE, layouts, push_k, shadow_extent, blending, &g_entities->rolling_player_model,
                                    true, 0.0f, dynamic, g_render_pass_manager->get(shadow_render_pass), 0, info);
        rolling_player_shadow_ppln->info = info;
        make_graphics_pipeline(rolling_player_shadow_ppln);
    }
}


internal_function void construct_player(player_t *player, player_create_info_t *info)
{
    persist_var vector4_t colors[player_color_t::INVALID_COLOR] = { vector4_t(0.0f, 0.0f, 0.7f, 1.0f),
                                                                    vector4_t(0.7f, 0.0f, 0.0f, 1.0f),
                                                                    vector4_t(0.4f, 0.4f, 0.4f, 1.0f),
                                                                    vector4_t(0.1f, 0.1f, 0.1f, 1.0f),
                                                                    vector4_t(0.0f, 0.7f, 0.0f, 1.0f) };
    
    player->id = info->name;
    player->ws_p = info->ws_position;
    player->ws_d = info->ws_direction;
    player->entering_acceleration = info->starting_velocity;
    player->size = info->ws_size;
    player->ws_r = info->ws_rotation;
    player->is_entering = 1;
    player->rolling_mode = 1;
    player->camera.camera = info->camera_info.camera_index;
    player->camera.is_third_person = info->camera_info.is_third_person;
    player->camera.distance_from_player = info->camera_info.distance_from_player;
    player->physics.enabled = info->physics_info.enabled;
    player->animation.cycles = info->animation_info.cycles;
    player->animation.animation_instance = initialize_animated_instance(get_global_command_pool(), info->animation_info.ubo_layout, info->animation_info.skeleton, info->animation_info.cycles);
    switch_to_cycle(&player->animation.animation_instance, player_t::animated_state_t::IDLE, 1);
    player->rendering.push_k.color = colors[info->color];
    player->rendering.push_k.roughness = 0.8f;
    player->rendering.push_k.metalness = 0.6f;
}


internal_function void initialize_players(input_state_t *input_state, application_type_t app_type)
{
    // This makes the this player the "main" player of the game, that the user is going to control with mouse and keyboard
    camera_handle_t main_camera = add_camera(input_state, get_backbuffer_resolution());
    bind_camera_to_3d_scene_output(main_camera);
    // -

    player_create_info_t main_player_create_info = {};
    main_player_create_info.name = "main"_hash;
    main_player_create_info.ws_position = vector3_t(-140, 140, -140);
    main_player_create_info.ws_direction = -glm::normalize(main_player_create_info.ws_position);
    main_player_create_info.ws_rotation = quaternion_t(glm::radians(45.0f), vector3_t(0, 1, 0));
    main_player_create_info.ws_size = vector3_t(2);
    main_player_create_info.starting_velocity = 5.0f;
    main_player_create_info.color = player_color_t::DARK_GRAY;
    main_player_create_info.physics_info.enabled = 1;
    main_player_create_info.terraform_power_info.speed = 700.0f;
    main_player_create_info.terraform_power_info.terraform_radius = 20.0f;
    main_player_create_info.camera_info.camera_index = main_camera;
    main_player_create_info.camera_info.is_third_person = 1;
    main_player_create_info.camera_info.distance_from_player = 15.0f;
    main_player_create_info.animation_info.ubo_layout = g_uniform_layout_manager->get(g_uniform_layout_manager->get_handle("uniform_layout.joint_ubo"_hash));
    main_player_create_info.animation_info.skeleton = &g_entities->player_mesh_skeleton;
    main_player_create_info.animation_info.cycles = &g_entities->player_mesh_cycles;

    player_t user;
    construct_player(&user, &main_player_create_info);

    player_handle_t user_handle = add_player(user);
    make_player_main(user_handle);
}


internal_function void render_world(uint32_t image_index, uint32_t current_frame, gpu_command_queue_t *queue)
{
    // Fetch some data needed to render
    auto transforms_ubo_uniform_groups = get_camera_transform_uniform_groups();
    shadow_display_t shadow_display_data = get_shadow_display();
    
    uniform_group_t uniform_groups[2] = {transforms_ubo_uniform_groups[image_index], shadow_display_data.texture};

    camera_t *camera = get_camera_bound_to_3d_output();

    // Rendering to the shadow map
    begin_shadow_offscreen(4000, 4000, queue);
    {
        auto *model_ppln = g_pipeline_manager->get(g_entities->player_shadow_ppln);
        g_entities->player_submission_queue.submit_queued_materials({1, &transforms_ubo_uniform_groups[image_index]}, model_ppln, queue, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        auto *rolling_model_ppln = g_pipeline_manager->get(g_entities->rolling_player_shadow_ppln);
        g_entities->rolling_player_submission_queue.submit_queued_materials({1, &transforms_ubo_uniform_groups[image_index]}, rolling_model_ppln, queue, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        g_voxel_chunks->gpu_queue.submit_queued_materials({1, &transforms_ubo_uniform_groups[image_index]}, g_pipeline_manager->get(g_voxel_chunks->chunk_mesh_shadow_pipeline), queue, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }
    end_shadow_offscreen(queue);

    // Rendering the scene with lighting and everything
    begin_deferred_rendering(image_index, queue);
    {
        auto *player_ppln = g_pipeline_manager->get(g_entities->player_ppln);
        auto *rolling_player_ppln = g_pipeline_manager->get(g_entities->rolling_player_ppln);
    
        g_entities->player_submission_queue.submit_queued_materials({2, uniform_groups}, player_ppln, queue, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        g_entities->rolling_player_submission_queue.submit_queued_materials({2, uniform_groups}, rolling_player_ppln, queue, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        g_voxel_chunks->gpu_queue.submit_queued_materials({2, uniform_groups}, g_pipeline_manager->get(g_voxel_chunks->chunk_mesh_pipeline), queue, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        g_entities->player_submission_queue.flush_queue();
        g_entities->rolling_player_submission_queue.flush_queue();
        
        render_3d_frustum_debug_information(&uniform_groups[0], queue, image_index, g_pipeline_manager->get(g_entities->dbg_hitbox_ppln));
        //dbg_render_chunk_edges(queue, &uniform_groups[0]);

        // ---- render skybox ----
        render_atmosphere({1, uniform_groups}, camera->p, queue);
    }
    end_deferred_rendering(camera->v_m, queue);

    apply_pfx_on_scene(queue, &transforms_ubo_uniform_groups[image_index], camera->v_m, camera->p_m);
}


internal_function void entry_point(void)
{
    // Load globals
    execute_lua("globals = require \"scripts/globals/globals\"");
    
    // Load startup code
    const char *startup_script = "scripts/sandbox/startup.lua";
    file_handle_t handle = create_file(startup_script, file_type_flags_t::TEXT | file_type_flags_t::ASSET);
    auto contents = read_file_tmp(handle);
    execute_lua((const char *)contents.content);
    remove_and_destroy_file(handle);

    // Execute startup code
    execute_lua("startup()");
}


void hard_initialize_world(input_state_t *input_state, VkCommandPool *cmdpool, application_type_t app_type, application_mode_t app_mode)
{
    add_global_to_lua(script_primitive_type_t::FUNCTION, "get_player_position", &lua_get_player_position);
    add_global_to_lua(script_primitive_type_t::FUNCTION, "set_player_position", &lua_set_player_position);
    add_global_to_lua(script_primitive_type_t::FUNCTION, "toggle_hit_box_display", &lua_toggle_collision_box_render);
    add_global_to_lua(script_primitive_type_t::FUNCTION, "render_direction_info", &lua_render_player_direction_information);
    add_global_to_lua(script_primitive_type_t::FUNCTION, "get_ts_view_dir", &lua_get_player_ts_view_direction);
    add_global_to_lua(script_primitive_type_t::FUNCTION, "stop_simulation", &lua_stop_simulation);
    add_global_to_lua(script_primitive_type_t::FUNCTION, "go_down", &lua_go_down);

    if (app_type == application_type_t::CONSOLE_APPLICATION_MODE)
    {
        add_global_to_lua(script_primitive_type_t::FUNCTION, "c_out", &lua_placeholder_c_out);
    }

    if (app_type == application_type_t::WINDOW_APPLICATION_MODE)
    {
        g_entities->rolling_player_submission_queue = make_gpu_material_submission_queue(10, VK_SHADER_STAGE_VERTEX_BIT, VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdpool);
        g_entities->player_submission_queue = make_gpu_material_submission_queue(20, VK_SHADER_STAGE_VERTEX_BIT, VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdpool);
    }

    if (app_type == application_type_t::WINDOW_APPLICATION_MODE)
    {
        initialize_entities_graphics_data(cmdpool, input_state);
    }

    hard_initialize_chunks();
    
    initialize_world(input_state, cmdpool, app_type, app_mode);
    
    clear_linear();
}


void initialize_world(input_state_t *input_state, VkCommandPool *cmdpool, application_type_t app_type, application_mode_t app_mode)
{
    g_voxel_chunks->size = 9.0f;
    g_voxel_chunks->grid_edge_size = 5;
    
    initialize_players(input_state, app_type);
    
    g_voxel_chunks->max_chunks = 20 * 20 * 20;
    g_voxel_chunks->chunks = (voxel_chunk_t **)allocate_free_list(sizeof(voxel_chunk_t *) * g_voxel_chunks->max_chunks);
    memset(g_voxel_chunks->chunks, 0, sizeof(voxel_chunk_t *) * g_voxel_chunks->max_chunks);

    uint32_t i = 0;
    
    for (uint32_t z = 0; z < g_voxel_chunks->grid_edge_size; ++z)
    {
        for (uint32_t y = 0; y < g_voxel_chunks->grid_edge_size; ++y)
        {
            for (uint32_t x = 0; x < g_voxel_chunks->grid_edge_size; ++x)
            {
                voxel_chunk_t **chunk_ptr = get_voxel_chunk((int32_t)i);
                *chunk_ptr = (voxel_chunk_t *)allocate_free_list(sizeof(voxel_chunk_t));
    
                initialize_chunk(*chunk_ptr, vector3_t(x, y, z) * (float32_t)(VOXEL_CHUNK_EDGE_LENGTH) - vector3_t((float32_t)g_voxel_chunks->grid_edge_size / 2) * (float32_t)(VOXEL_CHUNK_EDGE_LENGTH), ivector3_t(x, y, z));
                push_chunk_to_render_queue(*chunk_ptr);

                ++i;
            }    
        }    
    }

    construct_sphere(vector3_t(80.0f, 70.0f, 0.0f), 60.0f);
    construct_sphere(vector3_t(-80.0f, -50.0f, 0.0f), 120.0f);
    //construct_plane(vector3_t(0.0f), 60.0f);
}


internal_function void clean_up_entities(void)
{
    // Gets rid of all the entities, terrains, etc..., but not rendering stuff.
    g_entities->player_count = 0;
    
    /*for (uint32_t i = 0; i < g_entities->animation_component_count; ++i)
    {
        destroy_animated_instance(&g_entities->animation_components[i].animation_instance);
    }*/
    
    g_entities->main_player = -1;

    g_entities->name_map.clean_up();

    g_entities->rolling_player_submission_queue.mtrl_count = 0;
}


void clean_up_world_data(void)
{
    clean_up_entities();
}


void make_world_data(void /* Some kind of state */)
{
}


void update_network_world_state(void)
{
}


void sync_gpu_memory_with_world_state(gpu_command_queue_t *cmdbuf, uint32_t image_index)
{
    update_animation_gpu_data(cmdbuf);
    update_3d_output_camera_transforms(image_index);

    sync_gpu_with_chunk_state(cmdbuf);
}


void handle_all_input(input_state_t *input_state, float32_t dt, element_focus_t focus)
{
    // If world has focus
    if (focus == WORLD_3D_ELEMENT_FOCUS)
    {
        handle_world_input(input_state, dt);
        handle_input_debug(input_state, dt);
    }
}


void update_world(input_state_t *input_state,
                  float32_t dt,
                  uint32_t image_index,
                  uint32_t current_frame,
                  gpu_command_queue_t *cmdbuf,
                  application_type_t app_type,
                  element_focus_t focus)
{    
    switch (app_type)
    {
    case application_type_t::WINDOW_APPLICATION_MODE:
        {
            handle_all_input(input_state, dt, focus);
            
            update_entities(dt, app_type);

            sync_gpu_memory_with_world_state(cmdbuf, image_index);
    
            render_world(image_index, current_frame, cmdbuf);
        } break;
    case application_type_t::CONSOLE_APPLICATION_MODE:
        {
            update_entities(dt, app_type);
        } break;
    }
}


#include <glm/gtx/string_cast.hpp>


void handle_main_player_mouse_movement(player_t *e, uint32_t *action_flags, input_state_t *input_state, float32_t dt)
{
    if (input_state->cursor_moved)
    {
        vector3_t up = vector3_t(0.0f, 1.0f, 0.0f);
        
        // TODO: Make sensitivity configurable with a file or something, and later menu
        persist_var constexpr uint32_t SENSITIVITY = 15.0f;
    
        vector2_t prev_mp = vector2_t(input_state->previous_cursor_pos_x, input_state->previous_cursor_pos_y);
        vector2_t curr_mp = vector2_t(input_state->cursor_pos_x, input_state->cursor_pos_y);

        vector3_t res = e->ws_d;
	    
        vector2_t d = (curr_mp - prev_mp);

        float32_t x_angle = glm::radians(-d.x) * SENSITIVITY * dt;// *elapsed;
        float32_t y_angle = glm::radians(-d.y) * SENSITIVITY * dt;// *elapsed;
                
        res = matrix3_t(glm::rotate(x_angle, up)) * res;
        vector3_t rotate_y = glm::cross(res, up);
        res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

        res = glm::normalize(res);
                
        float32_t up_dot_view = glm::dot(up, res);
        float32_t minus_up_dot_view = glm::dot(-up, res);
                

        float32_t limit = 0.99f;
        if (up_dot_view > -limit && up_dot_view < limit && minus_up_dot_view > -limit && minus_up_dot_view < limit)
        {
            e->ws_d = res;
        }
        else
        {
        }
    }
}


void handle_main_player_mouse_button_input(player_t *e, uint32_t *action_flags, input_state_t *input_state, float32_t dt)
{
    if (input_state->mouse_buttons[mouse_button_type_t::MOUSE_RIGHT].is_down)
    {
        *action_flags |= (1 << action_flags_t::ACTION_TERRAFORM_ADD);
    }

    if (input_state->mouse_buttons[mouse_button_type_t::MOUSE_LEFT].is_down)
    {
        *action_flags |= (1 << action_flags_t::ACTION_TERRAFORM_DESTROY);
    }
}


void handle_main_player_keyboard_input(player_t *e, uint32_t *action_flags, physics_component_t *e_physics, input_state_t *input_state, float32_t dt)
{
    vector3_t up = vector3_t(0.0f, 1.0f, 0.0f);
    
    uint32_t movements = 0;
    float32_t accelerate = 1.0f;
    
    auto acc_v = [&movements, &accelerate](const vector3_t &d, vector3_t &dst){ ++movements; dst += d * accelerate; };

    vector3_t d = glm::normalize(vector3_t(e->ws_d.x,
                                           e->ws_d.y,
                                           e->ws_d.z));

    vector3_t res = {};

    *action_flags = 0;
    if (input_state->keyboard[keyboard_button_type_t::R].is_down) {accelerate = 6.0f; *action_flags |= (1 << action_flags_t::ACTION_RUN);}
    if (input_state->keyboard[keyboard_button_type_t::W].is_down) {acc_v(d, res); *action_flags |= (1 << action_flags_t::ACTION_FORWARD);}
    if (input_state->keyboard[keyboard_button_type_t::A].is_down) {acc_v(-glm::cross(d, up), res); *action_flags |= (1 << action_flags_t::ACTION_LEFT);}
    if (input_state->keyboard[keyboard_button_type_t::S].is_down) {acc_v(-d, res); *action_flags |= (1 << action_flags_t::ACTION_BACK);} 
    if (input_state->keyboard[keyboard_button_type_t::D].is_down) {acc_v(glm::cross(d, up), res); *action_flags |= (1 << action_flags_t::ACTION_RIGHT);}
    
    if (input_state->keyboard[keyboard_button_type_t::SPACE].is_down)
    {
        *action_flags |= (1 << action_flags_t::ACTION_UP);
    }
    
    if (input_state->keyboard[keyboard_button_type_t::LEFT_SHIFT].is_down)
    {
        acc_v(-up, res);
        *action_flags |= (1 << action_flags_t::ACTION_DOWN);
    }

    if (input_state->keyboard[keyboard_button_type_t::E].is_down && !e->toggled_rolling_previous_frame)
    {
        e->toggled_rolling_previous_frame = 1;
        e->rolling_mode ^= 1;
        if (!e->rolling_mode)
        {
            e->rolling_rotation = matrix4_t(1.0f);
            e->rolling_rotation_angle = 0.0f;
        }
    }
    else if (!input_state->keyboard[keyboard_button_type_t::E].is_down)
    {
        e->toggled_rolling_previous_frame = 0;
    }
            

    if (movements > 0)
    {
        res = res * 15.0f;

        e->ws_input_v = res;
    }
    else
    {
        e->ws_input_v = vector3_t(0.0f);
    }
}


void handle_main_player_action(input_state_t *input_state, float32_t dt)
{
    player_t *main_player = get_main_player();
    if (main_player)
    {
        player_t *e = main_player;
        physics_component_t *e_physics = &e->physics;

        handle_main_player_keyboard_input(e, &e->action_flags, e_physics, input_state, dt);
        handle_main_player_mouse_movement(e, &e->action_flags, input_state, dt);
        handle_main_player_mouse_button_input(e, &e->action_flags, input_state, dt);
    }
}


void handle_world_input(input_state_t *input_state, float32_t dt)
{
    handle_main_player_action(input_state, dt);
}


// Not to do with moving the player, just debug stuff : will be used later for stuff like opening menus
void handle_input_debug(input_state_t *input_state, float32_t dt)
{
    // ---- get bound player ----
    // TODO make sure to check if main_player < 0
    /*player_t *e_ptr = &g_entities->player_list[g_entities->main_player];
      camera_component_t *e_camera_component = &g_entities->camera_components[e_ptr->components.camera_component];
      physics_component_t *e_physics = &g_entities->physics_components[e_ptr->components.physics_component];
      camera_t *e_camera = get_camera(e_camera_component->camera);
      vector3_t up = e_ptr->on_t->ws_n;
    
      shadow_matrices_t shadow_data = get_shadow_matrices();
      shadow_debug_t    shadow_debug = get_shadow_debug();
    
      //    shadow_data.light_view_matrix = glm::lookAt(vector3_t(0.0f), -glm::normalize(light_pos), vector3_t(0.0f, 1.0f, 0.0f));

      if (input_state->keyboard[keyboard_button_type_t::P].is_down)
      {
      for (uint32_t i = 0; i < 8; ++i)
      {
      e_camera->captured_frustum_corners[i] = shadow_debug.frustum_corners[i];
      }

      e_camera->captured = 1;
      e_camera->captured_shadow_corners[0] = vector4_t(shadow_debug.x_min, shadow_debug.y_max, shadow_debug.z_min, 1.0f);
      e_camera->captured_shadow_corners[1] = vector4_t(shadow_debug.x_max, shadow_debug.y_max, shadow_debug.z_min, 1.0f);
      e_camera->captured_shadow_corners[2] = vector4_t(shadow_debug.x_max, shadow_debug.y_min, shadow_debug.z_min, 1.0f);
      e_camera->captured_shadow_corners[3] = vector4_t(shadow_debug.x_min, shadow_debug.y_min, shadow_debug.z_min, 1.0f);

      e_camera->captured_shadow_corners[4] = vector4_t(shadow_debug.x_min, shadow_debug.y_max, shadow_debug.z_max, 1.0f);
      e_camera->captured_shadow_corners[5] = vector4_t(shadow_debug.x_max, shadow_debug.y_max, shadow_debug.z_max, 1.0f);
      e_camera->captured_shadow_corners[6] = vector4_t(shadow_debug.x_max, shadow_debug.y_min, shadow_debug.z_max, 1.0f);
      e_camera->captured_shadow_corners[7] = vector4_t(shadow_debug.x_min, shadow_debug.y_min, shadow_debug.z_max, 1.0f);
      }*/
}


void destroy_world(void)
{
    g_render_pass_manager->clean_up();
    g_image_manager->clean_up();
    g_framebuffer_manager->clean_up();
    g_pipeline_manager->clean_up();
    g_gpu_buffer_manager->clean_up();

    destroy_graphics();
}


internal_function int32_t lua_get_player_position(lua_State *state)
{
    // For now, just sets the main player's position
    player_t *main_player = &g_entities->player_list[g_entities->main_player];
    lua_pushnumber(state, main_player->ws_p.x);
    lua_pushnumber(state, main_player->ws_p.y);
    lua_pushnumber(state, main_player->ws_p.z);
    return(3);
}


internal_function int32_t lua_set_player_position(lua_State *state)
{
    float32_t x = lua_tonumber(state, -3);
    float32_t y = lua_tonumber(state, -2);
    float32_t z = lua_tonumber(state, -1);
    player_t *main_player = &g_entities->player_list[g_entities->main_player];
    main_player->ws_p.x = x;
    main_player->ws_p.y = y;
    main_player->ws_p.z = z;
    return(0);
}


internal_function int32_t lua_toggle_collision_box_render(lua_State *state)
{
    g_entities->dbg.hit_box_display ^= true;
    return(0);
}


internal_function int32_t lua_render_player_direction_information(lua_State *state)
{
    const char *name = lua_tostring(state, -1);
    constant_string_t kname = make_constant_string(name, strlen(name));

    g_entities->dbg.render_sliding_vector_player = get_player(kname);

    persist_var char buffer[50];
    sprintf(buffer, "rendering for player: %s", name);
    console_out(buffer);
    
    return(0);
}


internal_function int32_t lua_set_veclocity_in_view_direction(lua_State *state)
{
    const char *name = lua_tostring(state, -2);
    float32_t velocity = lua_tonumber(state, -1);
    constant_string_t kname = make_constant_string(name, strlen(name));
    player_t *player = get_player(kname);
    player->ws_v += player->ws_d * velocity;
    return(0);
}


internal_function int32_t lua_get_player_ts_view_direction(lua_State *state)
{
    // For now, just sets the main player's position
    player_t *main_player = &g_entities->player_list[g_entities->main_player];
    //    vector4_t dir = glm::scale(main_player->on_t->size) * main_player->on_t->inverse_transform * vector4_t(main_player->ws_d, 0.0f);
    lua_pushnumber(state, main_player->ws_d.x);
    lua_pushnumber(state, main_player->ws_d.y);
    lua_pushnumber(state, main_player->ws_d.z);
    return(3);
}


internal_function int32_t lua_stop_simulation(lua_State *state)
{
    const char *name = lua_tostring(state, -1);
    constant_string_t kname = make_constant_string(name, strlen(name));

    player_t *player = get_player(kname);

    physics_component_t *component = &player->physics;
    component->enabled = false;
    //component->ws_velocity = vector3_t(0.0f);
    
    return(0);
}


internal_function int32_t lua_go_down(lua_State *state)
{
    player_t *main = get_main_player();
    auto *istate = get_input_state();
    istate->keyboard[keyboard_button_type_t::LEFT_SHIFT].is_down = is_down_t::REPEAT;
    istate->keyboard[keyboard_button_type_t::LEFT_SHIFT].down_amount += 1.0f / 60.0f;
    return(0);
}


void initialize_world_translation_unit(struct game_memory_t *memory)
{
    g_entities = &memory->world_state.entities;
    g_voxel_chunks = &memory->world_state.voxel_chunks;
}


internal_function int32_t lua_reinitialize(lua_State *state)
{
    return(0);
}
