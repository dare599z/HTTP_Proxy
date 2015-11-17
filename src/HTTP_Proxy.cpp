#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>

#include "Proxy.h"
#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP

/********************************
*
*
* Error codes and Other strings
*
*
********************************/

/********************************
*
*
* Structures and classes
*
*
********************************/

const static timeval tenSeconds = {10, 0}; // ten second timeout

/********************************
*
*
* Helper functions
*
*
********************************/

std::string
file_extension(const std::string& filename)
{
  std::string::size_type idx = filename.rfind('.');

  if (idx != std::string::npos)
  {
    return filename.substr(idx);
  }
  else
  {
    // No extension found
    return "";
  }
}

bool
file_exists(const std::string& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0); 
}

std::string
getCmdOption(const char ** begin, const char ** end, const std::string & option)
{
  const char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end)
  {
      return std::string(*itr);
  }
  return std::string();
}

bool
cmdOptionExists(const char** begin, const char** end, const std::string& option)
{
  return std::find(begin, end, option) != end;
}

std::string
MakeSuccessHeader(
  const std::string& mime_type,
  const size_t length,
  std::map<std::string, std::string>& other_attrs
  )
{
  std::string connection;
  if (other_attrs["Connection"].compare("keep-alive") == 0) {
    connection = "Connection: keep-alive\n";
  }
  else {
    connection = "Connection: close\n";
  }

  time_t rawtime;
  time(&rawtime);
  tm *gm = gmtime(&rawtime);

  char time_buffer[80];
  strftime(time_buffer, 80, "%a, %d %b %Y %H:%M:%S GMT\n", gm);

  std::ostringstream ss;
  ss << 
        "HTTP/1.1 200 OK\n" <<
        connection <<
        "Date: " << time_buffer <<
        "Content-Type: " << mime_type << "\n" <<
        "Content-Length: " << length << "\n"
  ;

  ss << "\n"; // have this last to separate the header from content
  return ss.str();
}

/********************************
*
*
* Callback functions
*
*
********************************/

// void
// callback_data_written(bufferevent *bev, void *context)
// {
//   connection_info *ci = reinterpret_cast<connection_info*>(context);
//   VLOG(1) << "Closing (WRITEOUT)";
//   close_connection(ci);
// }

void 
callback_accept_connection(
  evconnlistener *listener,
  evutil_socket_t newSocket,
  sockaddr *address,
  int socklen,
  void *context
  )
{
  event_base *base = evconnlistener_get_base(listener);
  bufferevent *bev = bufferevent_socket_new(base,
                                            newSocket,
                                            BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

  Proxy *p = new Proxy(base, bev, newSocket);

  VLOG(2) << "Opened connection.";
}

void
callback_accept_error(struct evconnlistener *listener, void *ctx)
{
  struct event_base *base = evconnlistener_get_base(listener);
  int err = EVUTIL_SOCKET_ERROR();
  LOG(FATAL) << "Got error <" << err << ": " << evutil_socket_error_to_string(err) << "> on the connection listener. Shutting down.";

  event_base_loopexit(base, NULL);
}

int
main(const int argc, const char** argv)
{
  // start the easylogging++.h library
  START_EASYLOGGINGPP(argc, argv); 
  el::Configurations conf;
  conf.setToDefault();
  conf.setGlobally(el::ConfigurationType::Format, "<%datetime{%H:%m:%s}><%levshort>: %msg");
  el::Loggers::reconfigureAllLoggers(conf);
  conf.clear();
  el::Loggers::addFlag( el::LoggingFlag::ColoredTerminalOutput );
  el::Loggers::addFlag( el::LoggingFlag::DisableApplicationAbortOnFatalLog );

  event_base *base = event_base_new();
  if ( !base )
  {
    LOG(FATAL) << "Error creating an event loop.. Exiting";
    return -2;
  }

  sockaddr_in incomingSocket; 
  memset(&incomingSocket, 0, sizeof incomingSocket);

  incomingSocket.sin_family = AF_INET;
  incomingSocket.sin_addr.s_addr = 0; // local host
  incomingSocket.sin_port = htons(80);

  evconnlistener *listener = evconnlistener_new_bind(
                                     base,
                                     callback_accept_connection,
                                     NULL,
                                     LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
                                     -1,
                                     (sockaddr*)&incomingSocket,
                                     sizeof incomingSocket
                                     );

  if ( !listener )
  {
    LOG(FATAL) << "Error creating a TCP socket listener.. Exiting.";
    return -3;
  }

  evconnlistener_set_error_cb(listener, callback_accept_error);
  event_base_dispatch(base);

  return 0;
}