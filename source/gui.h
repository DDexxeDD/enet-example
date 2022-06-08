#ifndef _gui_h
#define _gui_h

#include "nuklear.h"

#include "packet.h"
#include "client.h"
#include "server.h"

/*
 * the main ui function
 */
void draw_ui (struct nk_context* context, Client* clients, Server* server);
void draw_ui_shutdown (struct nk_context* context);

#endif
