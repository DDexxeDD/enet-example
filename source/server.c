#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "enet/enet.h"
#include "sokol_app.h"
#include "sokol_time.h"

#include "packet.h"
#include "server.h"

int server_initialize (Server* server)
{
	server->shutdown_server = false;
	server->shutdown_timeout = 0.0;
	server->frame_time = 0;
	server->last_frame_time = 0;
	server->state = SERVER_STATE_SHUTDOWN;

	for (int iter = 0; iter < SERVER_MAX_CLIENTS; iter++)
	{
		server->clients[iter].active = false;
		server->clients[iter].peer = NULL;
		memset (server->clients[iter].name, 0, SERVER_NAME_BUFFER_SIZE);
	}

	pthread_rwlock_init (&server->shutdown_lock, NULL);

	frame_limiter_initialize (&server->limiter);

	printf ("initialized server\n");

	return 0;
}

void* server_thread (void* data)
{
	ENetEvent event;
	Server* server = data;
	bool quit = false;

	printf ("server: launched\n");

	while (!quit)
	{
		frame_limiter_frame_start (&server->limiter);

		while (enet_host_service (server->host, &event, 0) > 0)
		{
			switch (event.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
					{
						printf ("server: connection from %x: %u\n", event.peer->address.host, event.peer->address.port);
					}
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					printf ("server: %x: %u disconnected\n", event.peer->address.host, event.peer->address.port);
					server_remove_client (server, server_find_client_by_peer (server, event.peer));
					enet_packet_destroy (event.packet);

					break;
				case ENET_EVENT_TYPE_RECEIVE:
				{
					// the first thing in a packet is its type
					uint8_t packet_type = (uint8_t) *event.packet->data;
					switch (packet_type)
					{
						case 0:
						{
							char name_buffer[SERVER_NAME_BUFFER_SIZE];
							strcpy (name_buffer, (char*) (&event.packet->data[1]));
							printf ("server: received greeting packet from client %s\n", name_buffer);
							server_add_client (server, event.peer, name_buffer);
							break;
						}
						case 1:
						{
							Packet_a* pack = (Packet_a*) &event.packet->data[sizeof (uint8_t)];
							Server_client* client = server_find_client_by_peer (server, event.peer);
							printf ("server: received packet from client %s, containing %i\n", client->name, pack->x);
							break;
						}
						case 2:
						{
							Packet_b* pack = (Packet_b*) &event.packet->data[sizeof (uint8_t)];
							Server_client* client = server_find_client_by_peer (server, event.peer);
							printf ("server: received global packet from client %s, contianing %i\n", client->name, pack->x);
							printf ("server: sending packet to all clients\n");
							server_send_packet_to_all (server);
							break;
						}
						default:
							break;
					}

					server->last_event.peer_id = event.peer->connectID;
					server->last_event.time_stamp = stm_now ();
					enet_packet_destroy (event.packet);
					break;
				}
				default:
					break;
			}
		}

		// pthread_rwlock_tryrdlock returns 0 on success
		// if the server cant get the read lock, just keep running
		// 	itll get there eventually (hopefully)
		if (pthread_rwlock_tryrdlock (&server->shutdown_lock) == 0)
		{
			quit = server->shutdown_server;
			pthread_rwlock_unlock (&server->shutdown_lock);
		}

		// lets not eat up 100% cpu
		frame_limiter_frame_end (&server->limiter, 100);
	}

	printf ("server: shutting down\n");

	// send gentle disconnect to peers, wait for their response
	if (server->client_count > 0)
	{
		for (int iter = 0; iter < SERVER_MAX_CLIENTS; iter++)
		{
			if (server->clients[iter].active)
			{
				printf ("server: sending disconnect to client %s\n", server->clients[iter].name);
				enet_peer_disconnect (server->clients[iter].peer, 0);
			}
		}

		server->shutdown_timeout = 3.0;
		while (server->client_count > 0 && server->shutdown_timeout > 0.0)
		{
			frame_limiter_frame_start (&server->limiter);
			server->frame_time = stm_laptime (&server->last_frame_time);

			if (server->shutdown_timeout > 0.0)
			{
				server->shutdown_timeout -= stm_sec (server->frame_time);
			}

			while (enet_host_service (server->host, &event, 0) > 0)
			{
				switch (event.type)
				{
					case ENET_EVENT_TYPE_DISCONNECT:
					{
						Server_client* client = server_find_client_by_peer (server, event.peer);
						if (client) // if we didnt find the client, dont do anything
						{
							printf ("server: received disconnect from client %s\n", client->name);
							server_remove_client (server, client);
						}
						// fallthrough on purpose
					}
					default:
						enet_packet_destroy (event.packet);
						break;
				}
			}

			frame_limiter_frame_end (&server->limiter, 10);
		}

		if (server->client_count > 0)
		{
			for (int iter = 0; iter < SERVER_MAX_CLIENTS; iter++)
			{
				if (server->clients[iter].active)
				{
					enet_peer_reset (server->clients[iter].peer);
				}
			}
		}
	}

	enet_host_destroy (server->host);

	server->state = SERVER_STATE_SHUTDOWN;

	return NULL;
}

int server_launch (Server* server)
{
	if (server->state == SERVER_STATE_RUNNING)
	{
		printf ("server: already launched\n");

		return 0;
	}

	ENetAddress address =
	{
		.host = ENET_HOST_ANY,
		.port = 2345
	};

	server->host = enet_host_create (&address, SERVER_MAX_CLIENTS, 2, 0, 0);

	if (!server->host)
	{
		printf ("failed to launch server\n");

		return 1;
	}

	server->shutdown_server = false;
	server->state = SERVER_STATE_SHUTDOWN;

	if (pthread_create (&server->thread, NULL, server_thread, server))
	{
		printf ("server: thread error\n");

		return 1;
	}

	server->state = SERVER_STATE_RUNNING;

	return 0;
}

void server_shutdown (Server* server)
{
	pthread_rwlock_wrlock (&server->shutdown_lock);
	server->shutdown_server = true;
	pthread_rwlock_unlock (&server->shutdown_lock);
}

void server_add_client (Server* server, ENetPeer* peer, const char* name)
{
	// find the first inactive client slot and put the client in there
	for (int iter = 0; iter < SERVER_MAX_CLIENTS; iter++)
	{
		Server_client* new_client = &server->clients[iter];

		if (!new_client->active)
		{
			new_client->active = true;
			new_client->peer = peer;
			strcpy (new_client->name, name);

			break;
		}
	}

	server->client_count++;
}

/*
 * scan the list of connected clients and find the one which is the peer being passed in
 *
 * ? is there a better way to connect clients sending packets and server client list ?
 */
Server_client* server_find_client_by_peer (Server* server, ENetPeer* peer)
{
	for (int iter = 0; iter < SERVER_MAX_CLIENTS; iter++)
	{
		if (server->clients[iter].active
			&& server->clients[iter].peer == peer)
		{
			return &server->clients[iter];
		}
	}

	return NULL;
}

void server_remove_client (Server* server, Server_client* client)
{
	client->active = false;
	client->peer = NULL;
	memset (client->name, 0, SERVER_NAME_BUFFER_SIZE);
	
	server->client_count--;
}

void server_send_packet_to_all (Server* server)
{
	Packet_d packet_to_all = {5};
	ENetPacket* packet = create_packet (4, &packet_to_all, sizeof (Packet_d));

	enet_host_broadcast (server->host, 0, packet);
	// the packet will be sent by enet_host_service at the beginning of server frame
}

void server_send_packet_to_one (Server* server, Server_client* client)
{
	Packet_c data = {8};
	ENetPacket* packet = create_packet (3, &data, sizeof (Packet_c));

	enet_peer_send (client->peer, 0, packet);
}
