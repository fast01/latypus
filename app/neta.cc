//
//  neta.cc
//
//  http application server
//

#include "latypus.h"

int main(int argc, const char * argv[])
{
    struct echo_fn {
        std::string operator()(http_server_connection *conn) {
            return std::string("echo ") + conn->request.get_request_path();
        }
    };
    
    protocol_engine engine;
    auto cfg = engine.default_config<http_server>();
    engine.bind_function<http_server>(cfg, "/echo", echo_fn());
    engine.run();
    engine.join();
    
    return 0;
}
