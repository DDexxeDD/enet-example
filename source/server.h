#ifndef server_h
#define server_h

#include <stdbool.h>
#include <pthread.h>

#include "enet/enet.h"

#include "packet.h"
#include "frame_limiter.h"

#define SERVER_MAX_CLIENTS 3
#define SERVER_NAME_BUFFER_SIZE 8
#define SERVER_MAX_NAME_LENGTH (SERVER_NAME_BUFFER_SIZE - 1)

typedef enum
{
	SERVER_STATE_SHUTDOWN,
	SERVER_STATE_RUNNING,
	SERVER_STATE_SHUTTING_DOWN
} Server_state;

typedef struct server_event_s
{
	unsigned int peer_id;
	uint64_t time_stamp;
} Server_event;

typedef struct server_client_s
{
	// since we are keeping connected clients in a static array
	// 	need to know if the slot in the array is in use or not (active)
	bool active;
	char name[SERVER_MAX_NAME_LENGTH];
	ENetPeer* peer;
} Server_client;

typedef struct server_s
{
	pthread_t thread;
	ENetHost* host;

	Server_client clients[SERVER_MAX_CLIENTS];
	int client_count;

	bool shutdown_server;
	pthread_rwlock_t shutdown_lock;
	double shutdown_timeout;

	Server_event last_event;
	Server_state state;

	uint64_t frame_time;
	uint64_t last_frame_time;

	Frame_limiter limiter;
} Server;

int server_initialize (Server* server);
void* server_thread (void* data);
int server_launch (Server* server);
void server_shutdown (Server* server);
Server_client* server_find_client_by_peer (Server* server, ENetPeer* peer);
void server_add_client (Server* server, ENetPeer* peer, const char* name);
void server_remove_client (Server* server, Server_client* client);
void server_send_packet_to_all (Server* server);
void server_send_packet_to_one (Server* server, Server_client* client);

#endif
