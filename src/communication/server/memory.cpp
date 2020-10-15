COMM_SEND(comm_server_memory_send) {
    ring_buffer<u8> *mem = (ring_buffer<u8> *)comm.handle;
    mem->write((u8 *)data, size);
}

COMM_RECV(comm_server_memory_recv) {
    ring_buffer<u8> *mem = (ring_buffer<u8> *)comm.handle;
    mem->read((u8 *)buffer, size);
}

void comm_server_memory_init(communication *comm, ring_buffer<u8> *comm_mem) {
    comm->handle = (uintptr_t)comm_mem;
    comm->send = &comm_server_memory_send;
    comm->recv = &comm_server_memory_recv;
}
