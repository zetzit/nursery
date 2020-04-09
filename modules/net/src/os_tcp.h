#if defined(__unix__) || defined(__APPLE__)
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static inline void os_net_tcp_close(io_Context *self) {
    if (!self->isvalid) {
        return;
    }
    close(self->fd);
    self->isvalid = false;
}

static inline io_Result os_net_tcp_send(
        net_tcp_Socket *self,
        err_Err *e, size_t et,
        const unsigned char * mem,
        size_t * memlen
)
{
    int r = send(
        self->ctx.fd,
        mem,
        *memlen,
#if defined(__linux__)
        MSG_NOSIGNAL
#else
        0
#endif
    );

    if (r < 0) {
        if (errno == EAGAIN) {
            return io_Result_Later;
        }
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_send", __LINE__, "senp");
        return io_Result_Error;
    }

    *memlen = (size_t)r;
    return io_Result_Ready;
}

static inline io_Result os_net_tcp_recv(
        net_tcp_Socket *self,
        err_Err *e, size_t et,
        unsigned char * mem,
        size_t * memlen
)
{
    if ((self->ctx).async != 0) {
        io_select(((self->ctx).async), e, et, &self->ctx, io_Ready_Read);
    }

    int r = recv(
        self->ctx.fd,
        mem,
        *memlen,
        0
    );

    if (r < 0) {
        if (errno == EAGAIN) {
            return io_Result_Later;
        }
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_recv", __LINE__, "recv");
        return io_Result_Error;
    }

    *memlen = (size_t)r;

    return io_Result_Ready;
}


static io_Result os_net_tcp_server_accept(
    net_tcp_server_Server *self,
    err_Err *e, size_t et,
    net_tcp_Socket *client
)
{

    if ((self->ctx).async != 0) {
        io_select(((self->ctx).async), e, et, &self->ctx, io_Ready_Read);
    }

    unsigned alen = sizeof(struct sockaddr_in6);

    int r = accept(
        self->ctx.fd,
        (struct sockaddr*)client->remote_addr.os,
        &alen);

    if (r < 0) {
        if (errno == EAGAIN) {
            return io_Result_Later;
        }
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_recvfrom", __LINE__, "accept");
        return io_Result_Error;
    }


    if (((struct sockaddr*)client->remote_addr.os)->sa_family  == AF_INET) {
        client->remote_addr.typ = net_address_Type_Ipv4;
    } else if (((struct sockaddr*)client->remote_addr.os)->sa_family  == AF_INET6) {
        client->remote_addr.typ = net_address_Type_Ipv6;
    }

    client->ctx.isvalid = true;
    client->ctx.fd      = r;
    client->ctx.async   = self->ctx.async;
    client->impl_send   = os_net_tcp_send;
    client->impl_recv   = os_net_tcp_recv;
    client->impl_close  = os_net_tcp_close;

    return io_Result_Ready;
}


static inline void os_net_tcp_server_listen(err_Err *e, size_t et, net_address_Address const* addr, net_tcp_server_Server *sock)
{

    size_t sockaddrsize = 0;
    switch (addr->typ) {
        case net_address_Type_Ipv6:
            sock->ctx.fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            sockaddrsize = sizeof(struct sockaddr_in6);
            break;
        case net_address_Type_Ipv4:
            sock->ctx.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddrsize = sizeof(struct sockaddr_in);
            break;
        default:
            break;
    }

    if (sock->ctx.fd < 0) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_open", __LINE__, "socket");
        return;
    }

    int y = 1;
    setsockopt(sock->ctx.fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));


    int r = bind(sock->ctx.fd, (struct  sockaddr*)(&addr->os), sockaddrsize);
    if (r != 0) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_open", __LINE__, "bind");
    }

    r = listen(sock->ctx.fd, 0);
    if (r != 0) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_open", __LINE__, "listen");
    }

    sock->impl_accept   = os_net_tcp_server_accept;
    sock->impl_close    = os_net_tcp_close;

    sock->ctx.isvalid = true;
}

static inline void os_net_tcp_server_make_async(err_Err *e, size_t et, net_tcp_server_Server *sock) {
    int flags = fcntl(sock->ctx.fd, F_GETFL, 0);
    if (flags == -1) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_make_async", __LINE__, "fcntl");
    }
    flags = flags | O_NONBLOCK;

    flags = fcntl(sock->ctx.fd, F_SETFL, flags);
    if (flags == -1) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_make_async", __LINE__, "fcntl");
    }
}

static inline void os_net_tcp_make_async(err_Err *e, size_t et, net_tcp_Socket *sock) {
    int flags = fcntl(sock->ctx.fd, F_GETFL, 0);
    if (flags == -1) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_make_async", __LINE__, "fcntl");
    }
    flags = flags | O_NONBLOCK;

    flags = fcntl(sock->ctx.fd, F_SETFL, flags);
    if (flags == -1) {
        err_fail_with_errno(e, et, __FILE__, "os_net_tcp_server_make_async", __LINE__, "fcntl");
    }
}
#else
void os_net_tcp_server_close(io_Context *self);
void os_net_tcp_server_listen(err_Err *e, size_t et, net_address_Address const* addr, net_tcp_server_Server *sock);
void os_net_tcp_server_make_async(err_Err *e, size_t et, net_tcp_server_Server *sock);
void os_net_tcp_make_async(err_Err *e, size_t et, net_tcp_server_Server *sock);
#endif
