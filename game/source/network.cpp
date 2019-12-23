// TODO: Find out why the hell the "next_value" of the voxels sent to the client not match the seemingly actual value of the voxels!!!

// TODO: Make sure that packet header's total packet size member matches the actual amount of bytes received by the socket

#include <cassert>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include "network.hpp"
#include "game.hpp"
#include "ui.hpp"
#include "script.hpp"
#include "core.hpp"
#include "thread_pool.hpp"


// Global
static network_state_t *g_network_state;


static void receiver_thread_process(void *receiver_thread_data)
{
    output_to_debug_console("Started receiver thread\n");
    
    receiver_thread_t *process_data = (receiver_thread_t *)receiver_thread_data;

    for (;;)
    {
        if (wait_for_mutex_and_own(process_data->mutex) && !process_data->receiver_freezed)
        {
            if (process_data->receiver_thread_loop_count == 0)
            {
            }
            
            ++(process_data->receiver_thread_loop_count);
            // ^^^^^^^^^^ debug stuff ^^^^^^^^^^^^

            char *message_buffer = (char *)process_data->packet_allocator.current;
        
            network_address_t received_address = {};
            int32_t bytes_received = receive_from(message_buffer, process_data->packet_allocator.capacity - process_data->packet_allocator.used_capacity, &received_address);

            if (bytes_received > 0)
            {
                // Actually officially allocate memory on linear allocator (even though it will already have been filled)
                process_data->packets[process_data->packet_count] = allocate_linear(bytes_received, 1, "", &process_data->packet_allocator);
                process_data->packet_sizes[process_data->packet_count] = bytes_received;
                process_data->addresses[process_data->packet_count] = received_address;

                ++(process_data->packet_count);
            }

            release_mutex(process_data->mutex);
        }
    }
}

static void initialize_receiver_thread(void)
{
    g_network_state->receiver_thread.packet_allocator.capacity = megabytes(30);
    g_network_state->receiver_thread.packet_allocator.start = g_network_state->receiver_thread.packet_allocator.current = malloc(g_network_state->receiver_thread.packet_allocator.capacity);

    g_network_state->receiver_thread.mutex = request_mutex();
    request_thread_for_process(&receiver_thread_process, &g_network_state->receiver_thread);
}

void initialize_as_server(void)
{
    initialize_socket_api(GAME_OUTPUT_PORT_SERVER);
}

void initialize_network_translation_unit(struct game_memory_t *memory)
{
    g_network_state = &memory->network_state;
}

// Adds a client to the network_component_t array in entities_t
uint32_t add_client(network_address_t network_address, const char *client_name, player_handle_t player_handle)
{
    // Initialize network component or something

    return(0);
}

client_t *get_client(uint32_t index)
{
    return(&g_network_state->clients[index]);
}

#define MAX_MESSAGE_BUFFER_SIZE 40000
static char message_buffer[MAX_MESSAGE_BUFFER_SIZE] = {};


void send_chunks_hard_update_packets(network_address_t address)
{
    serializer_t chunks_serializer = {};
    initialize_serializer(&chunks_serializer, sizeof(uint32_t) + (sizeof(uint8_t) * 3 + sizeof(uint8_t) * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH) * 8 /* Maximum amount of chunks to "hard update per packet" */);

    packet_header_t header = {};
    header.packet_mode = packet_mode_t::PM_SERVER_MODE;
    header.packet_type = server_packet_type_t::SPT_CHUNK_VOXELS_HARD_UPDATE;
    // TODO: Increment this with every packet sent
    header.current_tick = *get_current_tick();

    uint32_t hard_update_count = 0;
    voxel_chunk_values_packet_t *voxel_update_packets = initialize_chunk_values_packets(&hard_update_count);

    union
    {
        struct
        {
            uint32_t is_first: 1;
            uint32_t count: 31;
        };
        uint32_t to_update_count;
    } chunks_count;

    chunks_count.is_first = 0;
    chunks_count.count = hard_update_count;
    
    uint32_t loop_count = (hard_update_count / 8);
    for (uint32_t packet = 0; packet < loop_count; ++packet)
    {
        voxel_chunk_values_packet_t *pointer = &voxel_update_packets[packet * 8];

        header.total_packet_size = sizeof_packet_header() + sizeof(uint32_t) + sizeof(uint8_t) * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH * 8;
        serialize_packet_header(&chunks_serializer, &header);

        if (packet == 0)
        {
            chunks_count.is_first = 1;
            // This is the total amount of chunks that the client is waiting for
            serialize_uint32(&chunks_serializer, chunks_count.to_update_count);
            chunks_count.is_first = 0;
        }
        else
        {
            serialize_uint32(&chunks_serializer, chunks_count.to_update_count);
        }

        serialize_uint32(&chunks_serializer, 8);
        
        for (uint32_t chunk = 0; chunk < 8; ++chunk)
        {
            serialize_voxel_chunk_values_packet(&chunks_serializer, &pointer[chunk]);
        }

        send_serialized_message(&chunks_serializer, address);
        chunks_serializer.data_buffer_head = 0;

        hard_update_count -= 8;
    }

    if (hard_update_count)
    {
        voxel_chunk_values_packet_t *pointer = &voxel_update_packets[loop_count * 8];

        header.total_packet_size = sizeof_packet_header() + sizeof(uint8_t) * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH * VOXEL_CHUNK_EDGE_LENGTH * hard_update_count;
        serialize_packet_header(&chunks_serializer, &header);

        if (loop_count == 0)
        {
            chunks_count.is_first = 1;
            serialize_uint32(&chunks_serializer, chunks_count.to_update_count);
            chunks_count.is_first = 0;
        }
        else
        {
            serialize_uint32(&chunks_serializer, chunks_count.to_update_count);
        }
        
        serialize_uint32(&chunks_serializer, hard_update_count);
        
        for (uint32_t chunk = 0; chunk < hard_update_count; ++chunk)
        {
            serialize_voxel_chunk_values_packet(&chunks_serializer, &pointer[chunk]);
        }

        
        send_serialized_message(&chunks_serializer, address);
    }
}

void dispatch_newcoming_client_to_clients(uint32_t new_client_index)
{
    client_t *newcoming_client = get_client(new_client_index);
    player_t *player = get_player(newcoming_client->player_handle);
    
    serializer_t serializer = {};
    initialize_serializer(&serializer, 80);

    packet_header_t header = {};
    header.packet_mode = packet_mode_t::PM_SERVER_MODE;
    header.packet_type = server_packet_type_t::SPT_CLIENT_JOINED;
    header.current_tick = *get_current_tick();

    serialize_packet_header(&serializer, &header);

    player_state_initialize_packet_t player_initialize_packet = {};
    player_initialize_packet.client_id = new_client_index;
    player_initialize_packet.player_name = newcoming_client->name;
    player_initialize_packet.ws_position = player->ws_p;
    player_initialize_packet.ws_direction = player->ws_d;
    
    serialize_player_state_initialize_packet(&serializer, &player_initialize_packet);

    for (uint32_t client_index = 0; client_index < g_network_state->client_count; ++client_index)
    {
        client_t *current_client = get_client(client_index);

        send_serialized_message(&serializer, current_client->network_address);
    }
}

void dispatch_snapshot_to_clients(void)
{
    packet_header_t header = {};
    header.packet_mode = packet_mode_t::PM_SERVER_MODE;
    header.packet_type = server_packet_type_t::SPT_GAME_STATE_SNAPSHOT;

    // First game snapshot voxel deltas, then game snapshot players
    game_snapshot_voxel_delta_packet_t voxel_packet = {};
    game_snapshot_player_state_packet_t player_snapshots[MAX_CLIENTS] = {};

    // Prepare voxel delta snapshot packets
    uint32_t modified_chunks_count = 0;
    voxel_chunk_t **chunks = get_modified_voxel_chunks(&modified_chunks_count);

    voxel_packet.modified_count = modified_chunks_count;
    voxel_packet.modified_chunks = (modified_chunk_t *)allocate_linear(sizeof(modified_chunk_t) * modified_chunks_count);

    // FOR DEBUGGING
    uint32_t output_count = 0;
    
    for (uint32_t chunk_index = 0; chunk_index < modified_chunks_count; ++chunk_index)
    {
        voxel_chunk_t *chunk = chunks[chunk_index];
        modified_chunk_t *modified_chunk = &voxel_packet.modified_chunks[chunk_index];

        modified_chunk->chunk_index = convert_3d_to_1d_index(chunk->chunk_coord.x, chunk->chunk_coord.y, chunk->chunk_coord.z, get_chunk_grid_size());
        modified_chunk->modified_voxels = (modified_voxel_t *)allocate_linear(sizeof(modified_voxel_t) * chunk->modified_voxels_list_count);
        modified_chunk->modified_voxel_count = chunk->modified_voxels_list_count;
        for (uint32_t voxel = 0; voxel < chunk->modified_voxels_list_count; ++voxel)
        {
            uint16_t voxel_index = chunk->list_of_modified_voxels[voxel];
            modified_chunk->modified_voxels[voxel].previous_value = chunk->voxel_history[voxel_index];
            voxel_coordinate_t coord = convert_1d_to_3d_coord(voxel_index, VOXEL_CHUNK_EDGE_LENGTH);
            modified_chunk->modified_voxels[voxel].next_value = chunk->voxels[coord.x][coord.y][coord.z];
            modified_chunk->modified_voxels[voxel].index = voxel_index;

            ++output_count;
            output_to_debug_console((int32_t)(modified_chunk->modified_voxels[voxel].next_value), " ");
        }
    }

    output_to_debug_console(" -> ");
    
    // Prepare the player snapshot packets
    for (uint32_t client_index = 0; client_index < g_network_state->client_count; ++client_index)
    {
        client_t *client = get_client(client_index);
        player_t *player = get_player(client->player_handle);

        player_snapshots[client_index].client_id = client->client_id;
        player_snapshots[client_index].ws_position = player->ws_p;
        player_snapshots[client_index].ws_direction = player->ws_d;
        player_snapshots[client_index].ws_velocity = player->ws_v;
        player_snapshots[client_index].ws_up_vector = player->camera.ws_current_up_vector;
        player_snapshots[client_index].ws_rotation = player->ws_r;
        player_snapshots[client_index].action_flags = (uint32_t)player->previous_action_flags;

        player_snapshots[client_index].is_rolling = player->rolling_mode;
    }

    serializer_t out_serializer = {};
    initialize_serializer(&out_serializer,
                          sizeof_packet_header() +
                          sizeof(uint64_t) +
                          sizeof_game_snapshot_voxel_delta_packet(modified_chunks_count, voxel_packet.modified_chunks) +
                          sizeof_game_snapshot_player_state_packet() * g_network_state->client_count);

    // TODO: FIX THIS IS NOT THE ACTUAL PACKET SIZE: IT VARIES DEPENDING ON THE CLIENT
    header.total_packet_size = sizeof_packet_header() +
        sizeof(uint64_t) +
        sizeof_game_snapshot_voxel_delta_packet(modified_chunks_count, voxel_packet.modified_chunks) +
        sizeof_game_snapshot_player_state_packet() * g_network_state->client_count;
        
    serialize_packet_header(&out_serializer, &header);



    // These are the actual current voxel values
    serialize_game_snapshot_voxel_delta_packet(&out_serializer, &voxel_packet);


    
    uint32_t player_snapshots_start = out_serializer.data_buffer_head;

    // TODO: Find way so that sending packets to each client does not need packets to be reserialized
    for (uint32_t client_index = 0; client_index < g_network_state->client_count; ++client_index)
    {
        client_t *client = get_client(client_index);
        player_t *player = get_player(client->player_handle);

        if (client->received_input_commands)
        {
            game_snapshot_player_state_packet_t *player_snapshot_packet = &player_snapshots[client_index];

            out_serializer.data_buffer_head = player_snapshots_start;

            player_state_t *previous_received_player_state = &client->previous_received_player_state;

            uint64_t previous_received_tick = client->previous_client_tick;
            serialize_uint64(&out_serializer, previous_received_tick);

            // Now need to serialize the chunks / voxels that the client has modified, so that the client can remember which voxels it modified so that it does not do interpolation for the "correct" voxels that it just calculated
            serialize_uint32(&out_serializer, client->modified_chunks_count);

            bool force_client_to_do_voxel_correction = 0;
            for (uint32_t chunk = 0; chunk < client->modified_chunks_count; ++chunk)
            {
                serialize_uint16(&out_serializer, client->previous_received_voxel_modifications[chunk].chunk_index);
                serialize_uint32(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxel_count);

                client_modified_chunk_nl_t *modified_chunk_data = &client->previous_received_voxel_modifications[chunk];
                voxel_chunk_t *actual_voxel_chunk = *get_voxel_chunk(modified_chunk_data->chunk_index);
                
                for (uint32_t voxel = 0; voxel < client->previous_received_voxel_modifications[chunk].modified_voxel_count; ++voxel)
                {
                    local_client_modified_voxel_t *voxel_ptr = &modified_chunk_data->modified_voxels[voxel];
                    uint8_t actual_voxel_value = actual_voxel_chunk->voxels[voxel_ptr->x][voxel_ptr->y][voxel_ptr->z];

                    if (actual_voxel_value != voxel_ptr->value)
                    {
                        force_client_to_do_voxel_correction = 1;

                        client->needs_to_do_voxel_correction = 1;
                        client->needs_to_acknowledge_prediction_error = 1;
                        
                        serialize_uint8(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxels[voxel].x);
                        serialize_uint8(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxels[voxel].y);
                        serialize_uint8(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxels[voxel].z);
                        serialize_uint8(&out_serializer, actual_voxel_value);
                    }
                    // If the prediction was correct, do not force client to do the correction
                    else
                    {
                        serialize_uint8(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxels[voxel].x);
                        serialize_uint8(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxels[voxel].y);
                        serialize_uint8(&out_serializer, client->previous_received_voxel_modifications[chunk].modified_voxels[voxel].z);
                        serialize_uint8(&out_serializer, 255);
                    }
                }
            }

            if (force_client_to_do_voxel_correction)
            {
                output_to_debug_console("Client needs to do voxel correction: waiting for correction\n"); 
                            
                player_snapshot_packet->need_to_do_voxel_correction = 1;
                player_snapshot_packet->need_to_do_correction = 1;
                client->needs_to_acknowledge_prediction_error = 1;
            }
            
            for (uint32_t i = 0; i < g_network_state->client_count; ++i)
            {
                if (i == client_index)
                {
                    if (client->received_input_commands)
                    {
                        // Do comparison to determine if client correction is needed
                        float32_t precision = 0.1f;
                        vector3_t ws_position_difference = glm::abs(previous_received_player_state->ws_position - player_snapshot_packet->ws_position);
                        vector3_t ws_direction_difference = glm::abs(previous_received_player_state->ws_direction - player_snapshot_packet->ws_direction);

                        bool position_is_different = (ws_position_difference.x > precision ||
                                                      ws_position_difference.y > precision ||
                                                      ws_position_difference.z > precision);

                        bool direction_is_different = (ws_direction_difference.x > precision ||
                                                       ws_direction_difference.y > precision ||
                                                       ws_direction_difference.z > precision);

                        if (position_is_different)
                        {
                            output_to_debug_console("pos-");
                        }

                        if (direction_is_different)
                        {
                            output_to_debug_console("dir-");
                        }
                        
                        if (position_is_different || direction_is_different)
                        {
                            output_to_debug_console("correction-");
                            
                            // Make sure that server invalidates all packets previously sent by the client
                            player->network.player_states_cbuffer.tail = player->network.player_states_cbuffer.head;
                            player->network.player_states_cbuffer.head_tail_difference = 0;
                            
                            // Force client to do correction
                            player_snapshot_packet->need_to_do_correction = 1;

                            // Server will now wait until reception of a prediction error packet
                            client->needs_to_acknowledge_prediction_error = 1;
                        }
                        
                        player_snapshot_packet->is_to_ignore = 0;
                    }
                    else
                    {
                        player_snapshot_packet->is_to_ignore = 1;
                    }
                }

                serialize_game_snapshot_player_state_packet(&out_serializer, &player_snapshots[i]);
            }

            client->modified_chunks_count = 0;
            send_serialized_message(&out_serializer, client->network_address);
            output_to_debug_console(client->name, " ");
       }
    }

    output_to_debug_console("\n");

    clear_chunk_history_for_server();
}

float32_t get_snapshot_server_rate(void)
{
    return(g_network_state->server_game_state_snapshot_rate);
}

// Might have to be done on a separate thread just for updating world data
void update_as_server(input_state_t *input_state, float32_t dt)
{    
    //if (wait_for_mutex_and_own(g_network_state->receiver_thread.mutex))
    {
        // Send Snapshots (25 per second)
        // Every 40ms (25 sps) - basically every frame
        // TODO: Add function to send snapshot
        static float32_t time_since_previous_snapshot = 0.0f;
    
        // Send stuff out to the clients (game state and stuff...)
        // TODO: Add changeable
    
        time_since_previous_snapshot += dt;
        float32_t max_time = 1.0f / g_network_state->server_game_state_snapshot_rate; // 20 per second
        
        if (time_since_previous_snapshot > max_time)
        {
            // Dispath game state to all clients
            dispatch_snapshot_to_clients();
            
            time_since_previous_snapshot = 0.0f;
        }
        
        g_network_state->receiver_thread.receiver_thread_loop_count = 0;

        //        for (uint32_t packet_index = 0; packet_index < g_network_state->receiver_thread.packet_count; ++packet_index)
        for (uint32_t i = 0; i < 1 + 2 * g_network_state->client_count; ++i)
        {
            //network_address_t received_address = g_network_state->receiver_thread.addresses[packet_index];
            
            //serializer_t in_serializer = {};
            //in_serializer.data_buffer = (uint8_t *)(g_network_state->receiver_thread.packets[packet_index]);
            //in_serializer.data_buffer_size = g_network_state->receiver_thread.packet_sizes[packet_index];

            network_address_t received_address = {};
            int32_t bytes_received = receive_from(message_buffer, MAX_MESSAGE_BUFFER_SIZE, &received_address);

            if (bytes_received > 0)
            {
                serializer_t in_serializer = {};
                in_serializer.data_buffer = (uint8_t *)message_buffer;
                in_serializer.data_buffer_size = bytes_received;
                
                packet_header_t header = {};
                deserialize_packet_header(&in_serializer, &header);

                if (header.total_packet_size == in_serializer.data_buffer_size)
                {
                    if (header.packet_mode == packet_mode_t::PM_CLIENT_MODE)
                    {
                        switch (header.packet_type)
                        {
                        case client_packet_type_t::CPT_CLIENT_JOIN:
                            {

                                client_join_packet_t client_join = {};
                                deserialize_client_join_packet(&in_serializer, &client_join);

                                // Add client
                                client_t *client = get_client(g_network_state->client_count);
                                client->name = client_join.client_name;
                                client->client_id = g_network_state->client_count;
                                client->network_address = received_address;
                                client->current_packet_count = 0;

                                // Add the player to the actual entities list (spawn the player in the world)
                                client->player_handle = spawn_player(client->name, player_color_t::GRAY, client->client_id);
                                player_t *local_player_data_ptr = get_player(client->player_handle);
                                local_player_data_ptr->network.client_state_index = client->client_id;

                                ++g_network_state->client_count;
                                constant_string_t constant_str_name = make_constant_string(client_join.client_name, strlen(client_join.client_name));
                                g_network_state->client_table_by_name.insert(constant_str_name.hash, client->client_id);
                                g_network_state->client_table_by_address.insert(received_address.ipv4_address, client->client_id);
                        
                                // LOG:
                                console_out(client_join.client_name);
                                console_out(" joined the game!\n");
                        
                                // Reply - Create handshake packet
                                serializer_t out_serializer = {};
                                initialize_serializer(&out_serializer, 2000);
                                game_state_initialize_packet_t game_state_init_packet = {};
                                initialize_game_state_initialize_packet(&game_state_init_packet, client->client_id);


                        
                                packet_header_t handshake_header = {};
                                handshake_header.packet_mode = packet_mode_t::PM_SERVER_MODE;
                                handshake_header.packet_type = server_packet_type_t::SPT_SERVER_HANDSHAKE;
                                handshake_header.total_packet_size = sizeof_packet_header();
                                handshake_header.total_packet_size += sizeof(voxel_state_initialize_packet_t);
                                handshake_header.total_packet_size += sizeof(game_state_initialize_packet_t::client_index) + sizeof(game_state_initialize_packet_t::player_count);
                                handshake_header.total_packet_size += sizeof(player_state_initialize_packet_t) * game_state_init_packet.player_count;
                                handshake_header.current_tick = *get_current_tick();
                        
                                serialize_packet_header(&out_serializer, &handshake_header);
                                serialize_game_state_initialize_packet(&out_serializer, &game_state_init_packet);
                                send_serialized_message(&out_serializer, client->network_address);

                                send_chunks_hard_update_packets(client->network_address);

                                dispatch_newcoming_client_to_clients(client->client_id);
                        
                            } break;

                        case client_packet_type_t::CPT_INPUT_STATE:
                            {
                                client_t *client = get_client(header.client_id);
                                if (!client->needs_to_acknowledge_prediction_error)
                                {
                                    client->received_input_commands = 1;
                        
                                    // Current client tick (will be used for the snapshot that will be sent to the clients)
                                    // Clients will compare the state at the tick that the server recorded as being the last client tick at which server received input state (commands)
                                    client->previous_client_tick = header.current_tick;

                                    player_t *player = get_player(client->player_handle);
                        
                                    uint32_t player_state_count = deserialize_uint32(&in_serializer);

                                    player_state_t last_player_state = {};
                        
                                    for (uint32_t i = 0; i < player_state_count; ++i)
                                    {
                                        client_input_state_packet_t input_packet = {};
                                        player_state_t player_state = {};
                                        deserialize_client_input_state_packet(&in_serializer, &input_packet);

                                        player_state.action_flags = input_packet.action_flags;
                                        player_state.mouse_x_diff = input_packet.mouse_x_diff;
                                        player_state.mouse_y_diff = input_packet.mouse_y_diff;
                                        player_state.flags_byte = input_packet.flags_byte;
                                        player_state.dt = input_packet.dt;

                                        player->network.player_states_cbuffer.push_item(&player_state);

                                        last_player_state = player_state;
                                    }

                                    // Will use the data in here to check whether the client needs correction or not
                                    client->previous_received_player_state = last_player_state;

                                    client->previous_received_player_state.ws_position = deserialize_vector3(&in_serializer);
                                    client->previous_received_player_state.ws_direction = deserialize_vector3(&in_serializer);

                                    player->network.commands_to_flush += player_state_count;

                                    client_modified_voxels_packet_t voxel_packet = {};
                                    deserialize_client_modified_voxels_packet(&in_serializer, &voxel_packet);

                                    for (uint32_t i = 0; i < voxel_packet.modified_chunk_count; ++i)
                                    {
                                        client_modified_chunk_nl_t *chunk = &client->previous_received_voxel_modifications[i + client->modified_chunks_count];
                                        chunk->chunk_index = voxel_packet.modified_chunks[i].chunk_index;
                                        chunk->modified_voxel_count = voxel_packet.modified_chunks[i].modified_voxel_count;
                                        for (uint32_t voxel = 0; voxel < chunk->modified_voxel_count && voxel < 80; ++voxel)
                                        {
                                            chunk->modified_voxels[voxel].x = voxel_packet.modified_chunks[i].modified_voxels[voxel].x;
                                            chunk->modified_voxels[voxel].y = voxel_packet.modified_chunks[i].modified_voxels[voxel].y;
                                            chunk->modified_voxels[voxel].z = voxel_packet.modified_chunks[i].modified_voxels[voxel].z;
                                            chunk->modified_voxels[voxel].value = voxel_packet.modified_chunks[i].modified_voxels[voxel].value;
                                        }
                                    }
                                    client->modified_chunks_count += voxel_packet.modified_chunk_count;
                                }
                            } break;
                        case client_packet_type_t::CPT_PREDICTION_ERROR_CORRECTION:
                            {
                                client_t *client = get_client(header.client_id);
                                client->needs_to_acknowledge_prediction_error = 0;

                                player_t *player = get_player(client->player_handle);

                                client->previous_client_tick = deserialize_uint64(&in_serializer);
                                //client->received_input_commands = 0;
                            } break;
                        case client_packet_type_t::CPT_ACKNOWLEDGED_GAME_STATE_RECEPTION:
                            {
                                uint64_t game_state_acknowledged_tick = deserialize_uint64(&in_serializer);
                                client_t *client = get_client(header.client_id);
                        
                            } break;
                        }
                    }
                }
            }
        }

        //g_network_state->receiver_thread.packet_count = 0;
        //clear_linear(&g_network_state->receiver_thread.packet_allocator);
        //release_mutex(g_network_state->receiver_thread.mutex);
    }
}


void update_network_state(input_state_t *input_state, float32_t dt)
{
    switch(g_network_state->current_app_mode)
    {
    case application_mode_t::CLIENT_MODE: { tick_client(input_state, dt); } break;
    case application_mode_t::SERVER_MODE: { update_as_server(input_state, dt); } break;
    }
}

void initialize_network_state(game_memory_t *memory, application_mode_t app_mode)
{
    g_network_state->current_app_mode = app_mode;

    switch(g_network_state->current_app_mode)
    {
    case application_mode_t::CLIENT_MODE: { initialize_client(); } break;
    case application_mode_t::SERVER_MODE: { initialize_as_server(); } break;
    }
}
