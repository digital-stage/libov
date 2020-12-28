#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include "common.h"
#include <atomic>
#if defined(LINUX) || defined(linux) || defined(__APPLE__)
#include <netinet/ip.h>
#include <sys/socket.h>
#endif

#if defined(WIN32) || defined(UNDER_CE)
#include <winsock2.h>

#include <winsock.h>
#endif

#include <sys/types.h>

#include <string>
#include <unistd.h>

typedef struct sockaddr_in endpoint_t;

std::string addr2str(const struct in_addr& addr);
std::string ep2str(const endpoint_t& ep);
std::string ep2ipstr(const endpoint_t& ep);

std::string getmacaddr();
endpoint_t getipaddr();

class udpsocket_t {
public:
  udpsocket_t();
  ~udpsocket_t();
  void set_timeout_usec(int usec);
  port_t bind(port_t port, bool loopback = false);
  void destination(const char* host);
  ssize_t send(const char* buf, size_t len, int portno);
  ssize_t send(const char* buf, size_t len, const endpoint_t& ep);
  ssize_t recvfrom(char* buf, size_t len, endpoint_t& addr);
  endpoint_t getsockep();
  void close();
  const std::string addrname() const { return ep2str(serv_addr); };

private:
  int sockfd;
  endpoint_t serv_addr;
  bool isopen;

public:
  std::atomic_size_t tx_bytes;
  std::atomic_size_t rx_bytes;
};

class ovbox_udpsocket_t : public udpsocket_t {
public:
  ovbox_udpsocket_t(secret_t secret);
  void send_ping(stage_device_id_t cid, const endpoint_t& ep);
  void send_registration(stage_device_id_t cid, epmode_t, port_t port,
                         const endpoint_t& localep);
  char* recv_sec_msg(char* inputbuf, size_t& ilen, size_t& len,
                     stage_device_id_t& cid, port_t& destport, sequence_t& seq,
                     endpoint_t& addr);
  void set_secret(secret_t s) { secret = s; };

protected:
  secret_t secret;
};

#endif

/*
 * Local Variables:
 * mode: c++
 * compile-command: "make -C .."
 * End:
 */
