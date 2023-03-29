#include <flute/common/LogLine.h>
#include <flute/net/Reactor.h>
#include <flute/net/http/HttpRequest.h>
#include <flute/net/http/HttpResponse.h>
#include <flute/net/http/HttpServer.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

using namespace flute;

bool benchmark = false;
string BASE = "/home/frank/code/flute/root/";

std::string read_flie_as_string(const std::string& path) {
  std::ifstream infile(path);
  std::stringstream buffer;
  buffer << infile.rdbuf();
  return buffer.str();
}

bool has_source(const std::string& path) {
  struct stat buffer;
  int res = stat(path.c_str(), &buffer);
  return res == 0;
}

HttpResponseBodyType tell_request_type(const std::string& path) {
  if (path.find(".html") != std::string::npos)
    return kHtml;
  else if (path.find(".jpg") != std::string::npos)
    return kJPEG;
  else
    return kUnSupported;
}

size_t filesize_mb(const std::string& path) {
  struct stat stat_buf;
  int rc = stat(path.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size / (1024 * 1024) : -1;
}

// ================Main Functions================

void generate_response(const HttpRequest& req, HttpResponse* resp) {
  // LOG_TRACE << "Headers " << req.method_string() << " " << req.path();
  // const std::map<string, string>& headers = req.headers();
  // for (const auto& header : headers) {
  //   LOG_TRACE << header.first << ": " << header.second;
  // }

  string path = req.path();
  path = path.substr(path.find("/") + 1);
  if (path == "") path = "index.html";
  string full_path = BASE + path;
  LOG_TRACE << "Requesting [" << full_path << "]";
  HttpResponseBodyType request_type = tell_request_type(full_path);

  if (has_source(full_path)) {
    if (request_type == kHtml) {
      resp->set_status_code(HttpResponse::k200Ok);
      resp->set_status_message("OK");
      resp->set_content_type("text/html");
      resp->add_header("Server", "Flute:Muduo");
      string msg = read_flie_as_string(full_path);
      resp->set_body(msg);
    } else if (request_type == kJPEG) {
      resp->set_status_code(HttpResponse::k200Ok);
      resp->set_status_message("OK");
      resp->set_content_type("image/jpeg");
      resp->add_header("Server", "Flute:Muduo");
      resp->set_zerocopy(full_path);
    } else {
      assert(false);
    }
  } else {
    if (req.path() == "/time") {
      resp->set_status_code(HttpResponse::k200Ok);
      resp->set_status_message("OK");
      resp->set_content_type("text/html");
      resp->add_header("Server", "Flute:Muduo");
      resp->set_body("<h1>UTC on Flute Server</h1>\n" +
                     Timestamp::now().to_formatted_string() + "\n");
    } else {
      resp->set_status_code(HttpResponse::k404NotFound);
      resp->set_status_message("Not Found");
      resp->set_close_conn(true);
    }
  }
}

int main(int argc, char* argv[]) {
  LogLine::set_log_level(LogLine::TRACE);
  ZeroCopier::set_g_copy_mode(flute::ZeroCopier::kMMap);
  int num_threads = 0;
  if (argc > 1) {
    num_threads = atoi(argv[1]);
  }
  if (argc > 2) {
    LogLine::set_log_level(static_cast<LogLine::LogLevel>(atoi(argv[2])));
  }
  if (argc > 3) {
    ZeroCopier::set_g_copy_mode(
        static_cast<flute::ZeroCopier::CopyMode>(atoi(argv[3])));
  }
  if (argc > 4) {
    // argv[4] as chunk KB
    ZeroCopier::set_chunk_size(1024 * atoi(argv[4]));
  }
  Reactor reactor;
  HttpServer server(&reactor, InetAddress(8000), "dummy");
  server.set_response_callback(generate_response);
  server.set_thread_num(num_threads);
  server.start();
  reactor.loop();
}
