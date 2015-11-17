#ifndef __INCLUDE__PROXY__
#define __INCLUDE__PROXY__

#include <vector>
#include <string>

#include "Request.h"
#include "easylogging++.h"

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>


class Proxy {
public:
  Proxy(event_base *base, bufferevent *bev, evutil_socket_t socket);
  ~Proxy();

  void callback_read();
  static void callback_read(bufferevent *bev, void* caller) {
    reinterpret_cast<Proxy*>(caller)->callback_read();
  }

  void callback_events(short events);
  static void callback_events(bufferevent *bev, short events, void* caller) {
    reinterpret_cast<Proxy*>(caller)->callback_events(events);
  }

private:
  event_base *m_base;
  bufferevent *m_bev;
  evutil_socket_t m_socket;

  evbuffer *m_input, *m_output;

  std::vector<http_request> CreateRequests();
  static bool splitHeaders(const std::string &s, std::pair<std::string, std::string>& pair);

};

#endif 