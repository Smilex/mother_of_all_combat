struct communication;

#define COMM_SEND(_n) void _n(communication *comm, void *data, u32 size)
typedef COMM_SEND(comm_send_t);
#define COMM_RECV(_n) u32 _n(communication *comm, void *buffer, u32 size)
typedef COMM_RECV(comm_recv_t);

struct communication {
    uintptr_t handle;
    u32 amount_read;
    comm_send_t *send;
    comm_recv_t *recv;
};

enum comm_server_msg_names {
    INIT_MAP = 0,
};

struct comm_server_init_map {
    comm_server_msg_names name;
    u32 width, height;
};
