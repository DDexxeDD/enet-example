#include <string.h>

#include "enet/enet.h"

#include "packet.h"

/*
 * constructs packets specific to this program
 *
 * type is what the server will use to know what kind of packet this is
 */
ENetPacket* create_packet (uint8_t type, void* data, size_t data_size)
{
	ENetPacket* new_packet = enet_packet_create (&type, sizeof (uint8_t), ENET_PACKET_FLAG_RELIABLE);

	if (data)
	{
		enet_packet_resize (new_packet, sizeof (uint8_t) + data_size);
		memcpy (&new_packet->data[sizeof (uint8_t)], data, data_size);
	}

	return new_packet;
}

