COMM_SEND(comm_client_memory_send) {
    comm_memory_pipe *pipe = (comm_memory_pipe *)comm.handle;
    ring_buffer<u8> *mem = pipe->out;
    mem->write((u8 *)data, size);
}

COMM_RECV(comm_client_memory_recv) {
    comm_memory_pipe *pipe = (comm_memory_pipe *)comm.handle;
    ring_buffer<u8> *mem = pipe->in;
    mem->read((u8 *)buffer, size);
}

void comm_client_memory_init(communication *comm, comm_memory_pipe *comm_mem) {
    comm->handle = (uintptr_t)comm_mem;
    comm->send = &comm_client_memory_send;
    comm->recv = &comm_client_memory_recv;
}
