COMM_SEND(comm_server_memory_send) {
    comm_memory_pipe *pipe = (comm_memory_pipe *)comm.handle;
    ring_buffer<u8> *mem = pipe->out;
	mem->write((u8 *)&size, 4);
    mem->write((u8 *)data, size);
}

COMM_RECV(comm_server_memory_recv) {
    comm_memory_pipe *pipe = (comm_memory_pipe *)comm.handle;
    ring_buffer<u8> *mem = pipe->in;
	if (mem->distance() < 4) {
		return 0;
	}
	u32 packet_size, amount_to_read;
	mem->read((u8 *)&packet_size, 4);
	if (packet_size < size) {
		amount_to_read = packet_size;
	} else if (packet_size > size) {
		mem->add_to_read_it(packet_size);
		return 0;
	} else {
		amount_to_read = size;
	}

    return mem->read((u8 *)buffer, amount_to_read);
}

void comm_server_memory_init(communication *comm, comm_memory_pipe *comm_mem, memory_arena buffer) {
    comm->handle = (uintptr_t)comm_mem;
    comm->send = &comm_server_memory_send;
    comm->recv = &comm_server_memory_recv;

    comm->buffer = buffer;
    memory_arena_use(&comm->buffer, sizeof(comm_shared_header));
    comm->local_sequence_number = 0;
    comm->remote_sequence_number = 0;
}
