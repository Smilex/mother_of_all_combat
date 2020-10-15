COMM_SEND(comm_server_memory_send) {
    memory_arena *mem = (memory_arena *)comm->handle;
    u8 *ptr = memory_arena_use(mem, size);
    memcpy(ptr, data, size);
}

COMM_RECV(comm_server_memory_recv) {
    memory_arena *mem = (memory_arena *)comm->handle;
    u8 *ptr = mem->base + comm->amount_read;
    assert(comm->amount_read + size <= mem->max);
    memcpy(buffer, ptr, size);
}

void comm_server_memory_init(communication *comm, memory_arena *comm_mem) {
    comm->handle = (uintptr_t)comm_mem;
    comm->send = &comm_server_memory_send;
    comm->recv = &comm_server_memory_recv;
}
