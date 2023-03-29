#ifndef FLUTE_NET_SOCKETSOPS_H
#define FLUTE_NET_SOCKETSOPS_H

#include <arpa/inet.h>

namespace flute {

namespace socket_ops {

///
/// Creates a non-blocking socket file descriptor,
/// abort if any error.
int create_sockfd_nonblocking_or_abort(sa_family_t family);

int connect(int sockfd, const struct sockaddr* addr);
void bind_or_abort(int sockfd, const struct sockaddr* addr);
void listen_or_abort(int sockfd);
int accept(int sockfd, struct sockaddr_in6* addr);
ssize_t read(int sockfd, void* buf, size_t count);
ssize_t readv(int sockfd, const struct iovec* iov, int iovcnt);
ssize_t write(int sockfd, const void* buf, size_t count);
void close(int sockfd);
void shutdown_write(int sockfd);

void to_ip_port(char* buf, size_t size, const struct sockaddr* addr);
void to_ip(char* buf, size_t size, const struct sockaddr* addr);

void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr);

int getSocketError(int sockfd);

const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr);
const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

struct sockaddr_in6 get_local_addr(int sockfd);
struct sockaddr_in6 get_peer_addr(int sockfd);
bool isSelfConnect(int sockfd);

}  // namespace socket_ops

}  // namespace flute

#endif  // FLUTE_NET_SOCKETSOPS_H
