#ifndef __INCLUDE_HTTP_REQUEST__
#define __INCLUDE_HTTP_REQUEST__

#include <string>
#include <map>

class http_request {
public:
  http_request() {
    isValid = true;
  }

  bool keepAlive() {
    if (other_attrs["Connection"].compare("keep-alive") == 0) return true;
    else return false;
  }

  bool isValid;
  std::string error_string;
  std::string method;
  std::string uri;
  std::string http_version;
  std::map<std::string, std::string> other_attrs;

private:
};

#endif 
