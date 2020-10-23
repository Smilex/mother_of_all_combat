#define PROTOCOL_VERSION 0 

struct comm_memory_pipe {
    ring_buffer<u8> *in;
    ring_buffer<u8> *out;
};

struct comm_sent_packet {
    u32 when;
    u32 sequence;
    u32 retries;
    memory_arena mem;
};

struct communication;

#define COMM_SEND(_n) void _n(communication comm, void *data, u32 size)
typedef COMM_SEND(comm_send_t);
#define COMM_RECV(_n) u32 _n(communication comm, void *buffer, u32 size)
typedef COMM_RECV(comm_recv_t);

struct communication {
    uintptr_t handle;
    comm_send_t *send;
    comm_recv_t *recv;

    memory_arena buffer;
    u32 local_sequence_number,
        remote_sequence_number;
    u32 received_queue[33];
    doubly_linked_list<comm_sent_packet> sent_packets;
    s32 rtt;
    u32 last_sent_time;
};

struct comm_shared_header {
    u32 magic;
    u32 sequence;
    u32 ack;
    u32 ack_bitfield;
};

bool comm_flush(communication *comm) {
    u32 now = time_get_now_in_ms();
    for (u32 i = 0; i < comm->sent_packets.length(); ++i) {
        comm_sent_packet *packet = comm->sent_packets.get(i);
        u32 ms = now - packet->when;

        if (ms >= 1000) {
            comm->send(*comm, packet->mem.base, packet->mem.used);       
            comm->last_sent_time = now;
            packet->when = now;
            packet->retries++;
            if (packet->retries >= 5) {
                u32 len = comm->sent_packets.length();
                for (u32 i = 0; i < len; ++i) {
                    packet = comm->sent_packets.get(i);
                    assert(packet->mem.base);
                    free(packet->mem.base);
                }
                comm->sent_packets.clear();
                len = comm->sent_packets.length();
                return false;
            }
        }
    }

    if (comm->buffer.used == sizeof(comm_shared_header)) {
        return true;
    }

    comm_shared_header *header = (comm_shared_header *)comm->buffer.base;

    u32 magic = PROTOCOL_VERSION;
    magic |= (0b101 << 29);
    header->magic = magic;
    header->sequence = comm->local_sequence_number++;
    header->ack = comm->remote_sequence_number;

    header->ack_bitfield = 0;
    for (u32 i = 1; i < 33; ++i) {
        u32 seq = comm->remote_sequence_number - i;
        bool found = false;
        for (u32 j = 0; j < 33; ++j) {
            if (comm->received_queue[j] == seq) {
                found = true;
                break;
            }
        }

        if (found) {
            header->ack_bitfield |= 1 << (32 - i);
        }
    }

    comm_sent_packet packet;
    packet.sequence = header->sequence;
    packet.when = time_get_now_in_ms();
    packet.retries = 0;
    packet.mem.max = packet.mem.used = comm->buffer.used;
    packet.mem.base = (u8 *)malloc(packet.mem.max);
    assert(packet.mem.base);
    memcpy(packet.mem.base, comm->buffer.base, packet.mem.max);

    comm->sent_packets.push_back(packet);

    comm->last_sent_time = now;
    comm->send(*comm, comm->buffer.base, comm->buffer.used);
    comm->buffer.used = sizeof(comm_shared_header);

    return true;
}

void comm_write(communication *comm, void *data, u32 size) {
    u8 *ptr = memory_arena_use(&comm->buffer, size);
    memcpy(ptr, data, size);
}

u32 comm_read(communication *comm, void *buffer, u32 size) {
    u8 *buf = (u8 *)buffer;
    comm_shared_header *header;

    s32 len = comm->recv(*comm, buffer, size);

	if (len < 0) {
		return 0;
	}

    if (len < sizeof(header)) {
        return 0;
    }

	header = (comm_shared_header *)buffer;
    
    u32 version = header->magic ^ (0b101 << 29);
    if (version != PROTOCOL_VERSION) {
        return 0;
    }

    if (header->sequence > comm->remote_sequence_number) {
        comm->remote_sequence_number = header->sequence;
    }

    for (s32 i = 33 - 2; i >= 0; --i) {
        comm->received_queue[i + 1] = comm->received_queue[i];
    }
    comm->received_queue[0] = header->sequence;

    for (u32 i = 0; i < 33; ++i) {
        u32 ack = header->ack;
        if (i == 0 || header->ack_bitfield & (1 << (32 - i))) {
            ack -= i;
        } else
            continue;

        for (u32 j = 0; j < comm->sent_packets.length(); ++j) {
            comm_sent_packet *packet = comm->sent_packets.get(j);
            if (ack == packet->sequence) {
                u32 rtt = time_get_now_in_ms() - packet->when;
                real32 diff = (real32)rtt - (real32)comm->rtt;
                comm->rtt += (s32)(diff * 0.1);

                assert(packet->mem.base);
                free(packet->mem.base);
                comm->sent_packets.erase(j);
                break;
            }
        }
    }

	return len;
}

enum comm_server_msg_names {
    INIT_MAP = 0,
    DISCOVER,
    PING,
    DISCOVER_TOWN,
	YOUR_TURN,
    CONSTRUCTION_SET,
    ADD_UNIT
};

enum comm_client_msg_names {
    CONNECT = 0,
    START,
    PONG,
    ADMIN_DISCOVER_ENTIRE_MAP,
	END_TURN,
    SET_CONSTRUCTION
};

struct comm_client_header {
    comm_client_msg_names name;
};

struct comm_server_header {
    comm_server_msg_names name;
};

struct comm_server_init_map_body {
    u32 your_id;
    u32 width, height;
};

struct comm_server_discover_body {
    u32 num;
};

struct comm_server_discover_body_tile {
    v2<u32> position;
    terrain_names name;
};

struct comm_server_discover_town_body {
    u32 id;
    s32 owner;
    v2<u32> position;
};

struct comm_server_construction_set_body {
    u32 town_id;
    unit_names unit_name;
};

struct comm_client_set_construction_body {
    u32 town_id;
    unit_names unit_name;
};

struct comm_server_add_unit_body {
    unit_names unit_name;
    v2<u32> position;
};
