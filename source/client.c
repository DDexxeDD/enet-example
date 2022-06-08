#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "enet/enet.h"
#include "sokol_app.h"
#include "sokol_time.h"

#include "packet.h"
#include "client.h"
#include "frame_limiter.h"

/*
 * default initialization for a client
 */
int client_initialize (Client* client, const char* name)
{
	client->host = enet_host_create (NULL, 1, 2, 0, 0);

	if (!client->host)
	{
		printf ("failed to create client host\n");

		return 1;
	}

	client->remote_server = NULL;
	client->state = CLIENT_STATE_SHUTDOWN;
	client->waiting_for_response = false;
	client->response_timeout = 0;

	pthread_rwlock_init (&client->quit_lock, NULL);

	frame_limiter_initialize (&client->limiter);

	client->quit = false;
	client->test = false;
	client->thread_launched = false;

	memset (client->name, 0, CLIENT_NAME_BUFFER_SIZE);
	strcpy (client->name, name);

	return 0;
}

/*
 * connect a client to a server
 */
int client_connect (Client* client, const char* host_name)
{
	if (client->state == CLIENT_STATE_CONNECTED)
	{
		printf ("client: already connected\n");

		return 1;
	}

	enet_address_set_host (&client->address, host_name);
	client->address.port = 2345;

	client->remote_server = enet_host_connect (client->host, &client->address, 2, 0);
	client->state = CLIENT_STATE_CONNECTING;
	client->response_timeout = 3.0;
	client->waiting_for_response = true;

	return 0;
}

/*
 * disconnect a client from the server it is connected to
 *
 * wait_for_response will set the client to wait for a response from the server
 * 	or just immediately disconnect
 */
void client_disconnect (Client* client, bool wait_for_response)
{
	if (client->state == CLIENT_STATE_DISCONNECTED)
	{
		printf ("client: not connected\n");

		return;
	}

	if (client->state == CLIENT_STATE_CONNECTED)
	{
		enet_peer_disconnect (client->remote_server, 0);

		client->waiting_for_response = wait_for_response;

		if (wait_for_response)
		{
			client->state = CLIENT_STATE_DISCONNECTING;
			client->response_timeout = 3.0;
		}
		else
		{
			client->state = CLIENT_STATE_DISCONNECTED;
			client->response_timeout = 0.0;
			printf ("client %s: disconnected from server\n", client->name);
		}
	}
	else
	{
		printf ("client: requested disconnect, but is not connected\n");
	}
}

/*
 * send a packet of the given type to the server
 */
void client_send_packet (Client* client, uint8_t type)
{
	ENetPacket* packet = NULL;

	switch (type)
	{
		case 1:
		{
			Packet_a pack = {7};
			packet = create_packet (type, &pack, sizeof (Packet_a));
			enet_peer_send (client->remote_server, 0, packet);
			break;
		}
		case 2:
		{
			Packet_b pack = {7};
			packet = create_packet (type, &pack, sizeof (Packet_b));
			enet_peer_send (client->remote_server, 0, packet);
			break;
		}
		default:
			printf ("client %s trying to send unknown packet type %u", client->name, type);
			break;
	}
}

/*
 * the main function for client threads
 */
void* client_thread (void* data)
{
	Client* client = data;
	bool quit = false;

	// if the client is told to quit, it should also be told to disconnect first
	// 	so if the client is in a disconnecting state
	// 		keep running frames until it disconnects, or times out
	while (!quit || client->state == CLIENT_STATE_DISCONNECTING)
	{
		// lets not eat up 100% cpu
		frame_limiter_frame_start (&client->limiter);

		client->frame_time = stm_laptime (&client->last_frame_time);
		client_frame (client, client->frame_time);

		if (pthread_rwlock_tryrdlock (&client->quit_lock) == 0)
		{
			quit = client->quit;
			pthread_rwlock_unlock (&client->quit_lock);
		}

		frame_limiter_frame_end (&client->limiter, 100);
	}

	client->thread_launched = false;
	client->state = CLIENT_STATE_SHUTDOWN;

	return NULL;
}

/*
 * for actual main client function
 *
 * this is run directly on the main thread, for the main client
 */
void client_frame (Client* client, uint64_t frame_time)
{
	// if the client is waiting for a server response, reduce response timeout by frame time
	if (client->response_timeout > 0.0)
	{
		client->response_timeout -= stm_sec (frame_time);
	}

	ENetEvent event;
	while (enet_host_service (client->host, &event, 0) > 0)
	{
		switch (event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
				// if the client is waiting for this event
				if (client->state == CLIENT_STATE_CONNECTING)
				{
					uint8_t* ip = (uint8_t*) &event.peer->address.host;
					printf ("client: connected to %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
					client->state = CLIENT_STATE_CONNECTED;
					client->waiting_for_response = false;
					client->response_timeout = 0.0;

					// the server doesnt seem to register that a client is connected until
					// 	it receives a packet from the client

					// create and send greeting packet
					// 	which contains the client's name
					ENetPacket* packet_test = create_packet (0, client->name, strlen (client->name) + 1);
					enet_peer_send (client->remote_server, 0, packet_test);

					enet_host_flush (client->host);
				}
				// the client was not waiting for this event
				else
				{
					printf ("client: received connect packet from somewhere\n");
				}

				// whatever happened, destroy the packet
				enet_packet_destroy (event.packet);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				// the client told the server it was disconnecting
				if (client->state == CLIENT_STATE_DISCONNECTING)
				{
					printf ("client: received disconnect from server\n");
					client->state = CLIENT_STATE_DISCONNECTED;
				}
				// the server told the client to disconnect
				else
				{
					printf ("client %s: server told client to disconnect\n", client->name);
					client_disconnect (client, false);
				}

				enet_packet_destroy (event.packet);
				break;
			case ENET_EVENT_TYPE_RECEIVE:
			{
				switch ((uint8_t) *event.packet->data)
				{
					case 3:
						printf ("client %s: received spcific packet from server\n", client->name);
						break;
					case 4:
						printf ("client %s received global packet from server\n", client->name);
						break;
					default:
						printf ("client: received some packet from somewhere\n");
						break;
				}

				enet_packet_destroy (event.packet);
				break;
			}
			default:
				break;
		}
	}

	// client timed out waiting for a connect or disconnect response from the server
	if (client->waiting_for_response && client->response_timeout <= 0.0)
	{
		if (client->state == CLIENT_STATE_CONNECTING)
		{
			printf ("client: failed to connect to server\n");
		}
		else if (client->state == CLIENT_STATE_DISCONNECTING)
		{
			printf ("client: failed to receive disconnect from server, forcefully disconnecting\n");
		}

		enet_peer_reset (client->remote_server);
		client->remote_server = NULL;

		client->state = CLIENT_STATE_DISCONNECTED;
		client->waiting_for_response = false;
		client->response_timeout = 0.0;
	}
}

/*
 * launch a threaded client
 */
void client_launch (Client* client)
{
	if (client->thread_launched)
	{
		printf ("client already running\n");

		return;
	}

	pthread_rwlock_wrlock (&client->quit_lock);
	client->quit = false;
	pthread_rwlock_unlock (&client->quit_lock);

	if (pthread_create (&client->thread, NULL, client_thread, client))
	{
		printf ("failed to create client thread\n");

		return;
	}

	client->thread_launched = true;
}

/*
 * for shutting down client threads
 * should do nothing to a client running on the main thread
 */
void client_shutdown (Client* client)
{
	if (!client->test)
	{
		return;
	}

	// FIXME
	// 	need to be able to be shutting down and disconnected
	// 	this will only work correctly if the client is already disconnected
	if (client->state == CLIENT_STATE_CONNECTED)
	{
		client_disconnect (client, true);
	}
	else
	{
		client->state = CLIENT_STATE_SHUTTING_DOWN;
	}

	pthread_rwlock_wrlock (&client->quit_lock);
	client->quit = true;
	pthread_rwlock_unlock (&client->quit_lock);
}
