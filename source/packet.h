#ifndef demo_h
#define demo_h

#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>

#include "enet/enet.h"

// test packet for client to send to server
typedef struct packet_a_s
{
	int x;
} Packet_a;

// test packet for client to send to server
typedef struct packet_b_s
{
	int x;
} Packet_b;

// test packet for server to send to individual clients
typedef struct packet_c_s
{
	int x;
} Packet_c;

// test packet for server to send to all clients using enet_host_broadcast
typedef struct packet_d_s
{
	int x;
} Packet_d;

ENetPacket* create_packet (uint8_t type, void* data, size_t data_size);

#endif
