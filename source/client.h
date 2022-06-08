#ifndef client_h
#define client_h

#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include "enet/enet.h"

#include "packet.h"
#include "frame_limiter.h"

#define CLIENT_NAME_BUFFER_SIZE 8
#define CLIENT_MAX_NAME_LENGTH (CLIENT_NAME_BUFFER_SIZE - 1)

typedef enum
{
	CLIENT_STATE_CONNECTING,
	CLIENT_STATE_CONNECTED,
	CLIENT_STATE_DISCONNECTING,
	CLIENT_STATE_DISCONNECTED,
	CLIENT_STATE_SHUTTING_DOWN,
	CLIENT_STATE_SHUTDOWN
} Client_state;

typedef struct client_client_s
{
	char name[CLIENT_NAME_BUFFER_SIZE];

	ENetHost* host;  // the actual enet client
	ENetPeer* remote_server;  // the server the client is connected to
	ENetAddress address;

	Client_state state;
	bool waiting_for_response;  // is the client waiting for a response from the server
	double response_timeout;   // the time in seconds to wait for a repsonse from the server

	uint64_t last_frame_time;  // the time the last frame started
	uint64_t frame_time;  // delta time since the last frame started

	pthread_t thread;  // clients not running on the main thread keep their thread here
	pthread_rwlock_t quit_lock;  // used when changing the quit boolean

	Frame_limiter limiter;  // limit the frame rate of threaded clients

	bool quit;  // setting this to true will shut down a client thread
	bool test;  // true if this client is a threaded test client (not the main client)
	bool thread_launched;  // keep track of whether this client thread has been launched or not
} Client;

int client_initialize (Client* client, const char* name);
int client_connect (Client* client, const char* host_name);
void client_disconnect (Client* client, bool wait_for_response);
void client_send_packet (Client* client, uint8_t type);
void client_frame (Client* client, uint64_t frame_time);
void client_launch (Client* client);
void client_shutdown (Client* client);

#endif
