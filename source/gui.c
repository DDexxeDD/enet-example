#include <stdio.h>
#include <math.h>

#include "implementations.h"
#include "nuklear.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_nuklear.h"
#include "sokol_time.h"

#include "gui.h"
#include "client.h"
#include "server.h"

/*
 * draw a client window
 * 
 * connect/disconnect buttons
 * launch/shutdown button (if the client is not on the main thread)
 * the server the client is connected to
 * send packet button, which sends a demo packet to the server
 */
void draw_client_window (struct nk_context* context, Client* client, int id)
{
	char name_buffer[10];
	sprintf (name_buffer, "client %i", id);

	if (nk_begin (context, name_buffer, nk_rect (400, 10 + (160 * id), 380, 150), NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
	{
		nk_layout_row_static (context, 30, 90, 3);
		// if the client is a threaded client
		if (client->test)
		{
			if (client->thread_launched)
			{
				if (nk_button_label (context, "shutdown"))
				{
					client_shutdown (client);
				}
			}
			else
			{
				if (nk_button_label (context, "launch"))
				{
					client_launch (client);
				}
			}
		}
		// if a threaded client has been launched
		if (client->thread_launched)
		{
			if (nk_button_label (context, "connect"))
			{
				// XXX
				// 	for the main thread client, this is fine
				// 	threaded clients probably shouldnt be directly accessed like this
				client_connect (client, "localhost");
			}
			if (nk_button_label (context, "disconnect"))
			{
				client_disconnect (client, true);
			}

			// show what the client is doing
			nk_layout_row_static (context, 15, 200, 1);
			switch (client->state)
			{
				case CLIENT_STATE_CONNECTED:
					{
						// from https://www.daniweb.com/posts/jump/904337
						uint8_t* ip = (uint8_t*) &client->address.host;
						nk_labelf (context, NK_TEXT_LEFT, "connected to: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
						break;
					}
				case CLIENT_STATE_CONNECTING:
					nk_labelf (context, NK_TEXT_LEFT, "waiting for connection... %i", (int) ceil (client->response_timeout));
					break;
				case CLIENT_STATE_DISCONNECTING:
					nk_label (context, "disconnecting...", NK_TEXT_LEFT);
					break;
				case CLIENT_STATE_DISCONNECTED:
					nk_label (context, "no connection", NK_TEXT_LEFT);
					break;
				default:
					break;
			}

			// send a demo packet
			nk_layout_row_static (context, 30, 180, 2);
			if (client->host->connectedPeers > 0)
			{
				if (nk_button_label (context, "send packet"))
				{
					client_send_packet (client, 1);
				}
				if (nk_button_label (context, "send packet to all"))
				{
					client_send_packet (client, 2);
				}
			}
		}
	}
	nk_end (context);
}

/*
 * the main ui drawing function
 */
void draw_ui (struct nk_context* context, Client* clients, Server* server)
{
	static enum nk_style_header_align header_align = NK_HEADER_RIGHT;
	context->style.window.header.align = header_align;

	nk_flags window_flags = NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;

	// the server window
	if (nk_begin (context, "server", nk_rect (10, 10, 300, 500), window_flags))
	{
		nk_layout_row_static (context, 30, 200, 1);
		if (server->state == SERVER_STATE_RUNNING)
		{
			nk_label (context, "status: running", NK_TEXT_LEFT);
		}
		else
		{
			nk_label (context, "status: shutdown", NK_TEXT_LEFT);
		}

		nk_layout_row_static (context, 30, 90, 2);
		if (nk_button_label (context, "launch"))
		{
			if (server->state == SERVER_STATE_SHUTDOWN)
			{
				server_launch (server);
			}
		}
		if (nk_button_label (context, "shutdown"))
		{
			if (server->state == SERVER_STATE_RUNNING)
			{
				server_shutdown (server);
			}
		}

		nk_layout_row_dynamic (context, 30, 1);
		if (server->state == SERVER_STATE_RUNNING)
		{
			nk_labelf (context, NK_TEXT_LEFT, "connected clients: %i", server->client_count);

			if (nk_button_label (context, "send packet to all clients"))
			{
				server_send_packet_to_all (server);
			}

			if (server->client_count > 0)
			{
				for (int iter = 0; iter < SERVER_MAX_CLIENTS; iter++)
				{
					if (server->clients[iter].active)
					{
						nk_layout_row_dynamic (context, 30, 1);
						nk_labelf (context, NK_TEXT_LEFT, "client: %s", server->clients[iter].name);
						if (nk_button_label (context, "send packet"))
						{
							server_send_packet_to_one (server, &server->clients[iter]);
						}
						nk_layout_row_dynamic (context, 15, 1);
						nk_label (context, "", NK_TEXT_LEFT);
					}
				}
			}

			nk_layout_row_dynamic (context, 30, 1);
			nk_labelf (context, NK_TEXT_LEFT, "%f event from: %u", (float) stm_sec (server->last_event.time_stamp), server->last_event.peer_id);
		}
	}
	nk_end (context);

	// the client windows
	draw_client_window (context, &clients[0], 0);
	draw_client_window (context, &clients[1], 1);
	draw_client_window (context, &clients[2], 2);
}

void draw_ui_shutdown (struct nk_context* context)
{
	if (nk_begin (context, "Shutdown", nk_rect (10, 10, 300, 400), NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
	{
		nk_layout_row_static (context, 30, 200, 1);
		nk_label (context, "shutting down...", NK_TEXT_LEFT);
	}
	nk_end (context);
}
