#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <locale.h>

#include "enet/enet.h"
#include "implementations.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_time.h"
#include "nuklear.h"
#include "sokol_nuklear.h"

#include "packet.h"
#include "server.h"
#include "client.h"
#include "gui.h"

// clients[0] runs on the main thread
// all other clients run on their own threads
static Client clients[3];
// the server will run on its own thread
static Server server;
// keep track of how long each frame takes to process
static uint64_t last_frame_time;
static uint64_t frame_time;
// sokol (currently) forces vsync, so this time will be pretty similar from frame to frame
// 	but dont expect to always be the same ;)

bool shutdown_everything;

/*
 * sokol init callback
 */
void initialize (void)
{
	shutdown_everything = false;
	last_frame_time = 0;
	frame_time = 0;

	// this needs to be done before _any_ other enet thing
	if (enet_initialize () != 0)
	{
		printf ("an error occured while initializing enet\n");
		sapp_request_quit ();
	}

	// setup sokol_time
	stm_setup ();

	// setup sokol_gfx
	sg_setup
	(
		&(sg_desc)
		{
			.context = sapp_sgcontext ()
		}
	);

	// set up the server and clients
	server_initialize (&server);
	client_initialize (&clients[0], "0");
	client_initialize (&clients[1], "1");
	client_initialize (&clients[2], "2");

	// this makes drawing the gui a smidge easier
	clients[0].thread_launched = true;
	clients[1].test = true;
	clients[2].test = true;

	// set up sokol_nuklear
	snk_setup
	(
		&(snk_desc_t)
		{
			.dpi_scale = sapp_dpi_scale ()
		}
	);
}

/*
 * sokol frame callback
 * this is where you do most of everything
 */
void frame (void)
{
	frame_time = stm_laptime (&last_frame_time);

	struct nk_context* context = snk_new_frame ();

	if (shutdown_everything)
	{
		draw_ui_shutdown (context);
	}
	else
	{
		draw_ui (context, clients, &server);
	}

	// the screen clear color, the background color
	const sg_pass_action pass_action =
	{
		.colors[0] =
		{
			.action = SG_ACTION_CLEAR, .value = {0.1f, 0.1f, 0.1f, 0.0f}
		}
	};

	sg_begin_default_pass (&pass_action, sapp_width (), sapp_height ());
	snk_render (sapp_width (), sapp_height ());
	sg_end_pass ();
	sg_commit ();

	// this is the main client, running on the main thread
	client_frame (&clients[0], frame_time);

	if (shutdown_everything && server.state == SERVER_STATE_SHUTDOWN)
	{
		if (clients[1].state != CLIENT_STATE_SHUTTING_DOWN
			&& clients[1].state != CLIENT_STATE_SHUTDOWN)
		{
			client_shutdown (&clients[1]);
		}

		if (clients[2].state != CLIENT_STATE_SHUTTING_DOWN
			&& clients[2].state != CLIENT_STATE_SHUTDOWN)
		{
			client_shutdown (&clients[2]);
		}
	}

	if (shutdown_everything
		&& clients[1].state == CLIENT_STATE_SHUTDOWN
		&& clients[2].state == CLIENT_STATE_SHUTDOWN
		&& server.state == SERVER_STATE_SHUTDOWN)
	{
		sapp_request_quit ();
	}
}

/*
 * sokol cleanup callback
 */
void cleanup (void)
{
	// join up all the threads and shut everything down
	pthread_join (clients[1].thread, NULL);
	pthread_join (clients[2].thread, NULL);
	pthread_join (server.thread, NULL);
	enet_deinitialize ();
	snk_shutdown ();
	sg_shutdown ();
}

void start_shutdown ()
{
	pthread_rwlock_wrlock (&server.shutdown_lock);
	server.shutdown_server = true;
	pthread_rwlock_unlock (&server.shutdown_lock);

	shutdown_everything = true;
}

/*
 * sokol event callback
 */
void input (const sapp_event* event)
{
	// handle sokol_nuklear events
	snk_handle_event (event);

	switch (event->type)
	{
		case SAPP_EVENTTYPE_QUIT_REQUESTED:
			start_shutdown ();
			break;
		case SAPP_EVENTTYPE_KEY_DOWN:
			switch (event->key_code)
			{
				case SAPP_KEYCODE_ESCAPE:
					start_shutdown ();
					break;
				default:
					break;
			}
		default:
			break;
	}
}

/*
 * the function that sets up sokol_app
 */
sapp_desc sokol_main (int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	return (sapp_desc)
	{
		.init_cb = initialize,
		.frame_cb = frame,
		.cleanup_cb = cleanup,
		.event_cb = input,
		.enable_clipboard = true,
		.width = 800,
		.height = 600,
		.window_title = "enet test",
		.ios_keyboard_resizes_canvas = true,
		.icon.sokol_default = true,
	};
}
