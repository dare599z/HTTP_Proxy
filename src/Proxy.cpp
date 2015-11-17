#include "Proxy.h"

Proxy::Proxy(event_base *base, bufferevent *bev, evutil_socket_t socket) :
  m_base(base),
  m_bev(bev),
  m_socket(socket)
{
  m_input = bufferevent_get_input(m_bev);
  m_output = bufferevent_get_output(m_bev);

  bufferevent_setcb(m_bev, callback_read, NULL, callback_events, this);
  bufferevent_setwatermark(m_bev, EV_WRITE, 0, 0);
  bufferevent_enable(m_bev, EV_READ|EV_WRITE);
}

Proxy::~Proxy()
{

}

void
Proxy::callback_events(short events)
{

}

void
Proxy::callback_read()
{
  bool keepAlive = false;

  /*
    Go create any HTTP requests that may have been
    pipelined, parse them out, and get a vector of
    them
  */

  std::vector<http_request> requests = CreateRequests();

  //LOG(DEBUG) << "number of requests to service= " << requests.size();
  
  for ( auto request : requests )
  {
    //LOG(DEBUG) << "Servicing request.. ";
    // http_request &req = *it;

    if ( !request.isValid ) {
      LOG(ERROR) << "Request not valid.";
    }

    if ( !(request.http_version.compare("HTTP/1.0") == 0) && !(request.http_version.compare("HTTP/1.1") == 0) ) {
      // std::string e = Make400("Invalid HTTP-Version: ", request.http_version);
      // LOG(WARNING) << "<400>: " << e;
      // bufferevent_write( m_bev, e.c_str(), e.length() );
      continue;
    }

    if (request.method.compare("GET") == 0)
    {
      // std::string extension = file_extension(req.uri());
      // VLOG(2) << "Requested extension: " << extension;
      // std::map<std::string, std::string>::iterator f_it = sc.m_file_types.find(extension);
      // std::string header = MakeSuccessHeader(f_it->second, fd_stat.st_size, req.other_attrs);

      if ( request.keepAlive() )
      {
        // LOG(INFO) << "<200>: " << req.uri() << " ~ (KEEP-ALIVE)";
        keepAlive = true;
      }
      else
      {
        // LOG(INFO) << "<200>: " << req.uri() << " ~ (CLOSE)";
        keepAlive = false;
      }
    } // GET method
    // else {
    //   std::string e = Make400("Invalid Method: ", req.method);
    //   LOG(WARNING) << "<400>: Invalid Method: " << req.method;
    //   bufferevent_write( m_bev, e.c_str(), e.length() );
    //   if ( req.keepAlive() ) keepAlive = true;
    //   continue;
    // }
  }

  // After we have processed and responded to all of the requests,
  // we need to figure out what to do with the connection..
  // bufferevent_free(ev);
  if (keepAlive)
  {
    VLOG(3) << "Keep-alive = true";
    // event_add(ci->timeout_event, &tenSeconds);
  }
  else
  {
    VLOG(3) << "Keep-alive = false";
    bufferevent_setcb(m_bev, callback_read, /*callback_data_written*/ NULL, callback_events, this);
  }
}

bool
Proxy::splitHeaders(const std::string &s, std::pair<std::string, std::string>& pair)
{
  std::stringstream ss(s);
  std::string item;

  // get the key
  std::getline(ss, item, ':');
  if (item.compare("") == 0) return false; //blank key
  pair.first = item;

  // get the value, and strip the space
  std::getline(ss, item);
  if (item.at(0) == ' ') {
    item.erase(0,1);
  }
  pair.second = item;
  return true;
}

std::vector<http_request>
Proxy::CreateRequests()
{
  std::vector<http_request> requests;
  http_request req;

  for (int i = 1; ; ++i)
  {
    size_t n;
    char *line = evbuffer_readln(m_input, &n, EVBUFFER_EOL_CRLF);
    
    if ( !line ) {
      if ( i == 1 ) {
        VLOG(3) << "Total pipelined requests: " << requests.size();
        return requests;
      }
      VLOG(3) << "Middle of parsing request, blank line.";
      goto begin_next;
    }
    if ( i == 1 ) {
      // First line of HTTP Request...
      // Should have METHOD URI HTTP_VERSION
      std::istringstream ss(line);

      // Parse out the method
      if ( !(ss>>req.method) ) {
        LOG(WARNING) << "Unable to parse HTTP Method.";
        LOG(WARNING) << "\t[" << line << "]";
        req.isValid = false;
        req.error_string = "Unable to parse HTTP Method.";
        goto begin_next;
      }
      VLOG(2) << "req.method= " << req.method;

      // Parse out the uri
      if ( !(ss>>req.uri) ) {
        LOG(WARNING) << "Unable to parse HTTP URI.";
        LOG(WARNING) << "\t[" << line << "]";
        req.isValid = false;
        req.error_string = "Unable to parse HTTP URI.";
        goto begin_next;
      }
      VLOG(2) << "req.uri= " << req.uri;

      // Parse out the http version
      if ( !(ss>>req.http_version) ) {
        LOG(WARNING) << "Unable to parse HTTP Version.";
        LOG(WARNING) << "\t[" << line << "]";
        req.isValid = false;
        req.error_string = "Unable to parse HTTP Version.";
        goto begin_next;
      }
      VLOG(2) << "req.http_version= " << req.http_version;
    }
    else {
      // We need to try and see if the client is pipelining
      // requests, and if they are, it's probabaly separated
      // by a newline here.. Reset the count back to one
      if (strlen(line) == 0) goto begin_next;

      /*
      This is any other content sent along with the request,
      such as Connection: keep-alive
      */

      std::pair<std::string, std::string> pair;
      bool b = splitHeaders(line, pair);
      if (b)
      {
        req.other_attrs.insert(pair);
        VLOG(3) << "req.other_attrs[" << pair.first << "]= " << pair.second;
      }
      continue;
    }
    continue;

    // This begin_next goto statement happens when a request is followed
    // up with a blankline
    begin_next:
      //LOG(DEBUG) << "[[" << __LINE__ << "]]: " << "starting new request";
      requests.push_back(req);
      req = http_request();
      i=0;
      continue;
  }
  return requests;
}