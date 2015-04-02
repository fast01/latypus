//
//  http_cient.cc
//

#include "plat_os.h"
#include "plat_net.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <deque>
#include <map>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "io.h"
#include "url.h"
#include "log.h"
#include "socket.h"
#include "resolver.h"
#include "config_parser.h"
#include "config.h"
#include "pollset.h"
#include "protocol.h"
#include "connection.h"
#include "connection_tcp.h"
#include "protocol_thread.h"
#include "protocol_engine.h"
#include "protocol_connection.h"

#include "http_common.h"
#include "http_constants.h"
#include "http_parser.h"
#include "http_request.h"
#include "http_response.h"
#include "http_client.h"


// sock
protocol_sock http_client::client_sock_tcp_connection
    (get_proto(), "tcp_connection", protocol_sock_tcp_connection);
protocol_sock http_client::client_sock_tcp_tls_connection
    (get_proto(), "tcp_tls_connection", protocol_sock_tcp_connection | protocol_sock_tcp_tls);

// actions
protocol_action http_client::action_connect_host
    (get_proto(), "connect_host", &connect_host);
protocol_action http_client::action_process_next_request
    (get_proto(), "process_next_request", &process_next_request);
protocol_action http_client::action_keepalive_wait_connection
    (get_proto(), "keepalive_wait_connection", &keepalive_wait_connection);

// threads
protocol_mask http_client::thread_mask_connect
    (get_proto(), "connect");
protocol_mask http_client::thread_mask_processor
    (get_proto(), "processor");
protocol_mask http_client::thread_mask_keepalive
    (get_proto(), "keepalive");

// states
protocol_state http_client::connection_state_free
    (get_proto(), "free");
protocol_state http_client::connection_state_client_request
    (get_proto(), "client_request", &handle_state_client_request);
protocol_state http_client::connection_state_client_body
    (get_proto(), "client_body", &handle_state_client_body);
protocol_state http_client::connection_state_server_response
    (get_proto(), "server_response", &handle_state_server_response);
protocol_state http_client::connection_state_server_body
    (get_proto(), "server_body", &handle_state_server_body);
protocol_state http_client::connection_state_waiting
    (get_proto(), "waiting", &handle_state_waiting);


/* http_client_request */

http_client_request::http_client_request(HTTPMethod method, url_ptr url,
                                                           http_client_handler_ptr handler)
: method(method), url(url), handler(handler) {}


/* http_client_connection */

template <>
int http_client_connection_tmpl<connection_tcp>::get_poll_fd()
{
    return conn.get_poll_fd();
}

template <>
poll_object_type http_client_connection_tmpl<connection_tcp>::get_poll_type()
{
    return http_client::client_sock_tcp_connection.type;
}

template <>
bool http_client_connection_tmpl<connection_tcp>::init(protocol_engine_delegate *delegate)
{
    buffer.reset();
    conn.reset();
    request.reset();
    response.reset();
    request_has_body = false;
    response_has_body = false;
    connection_close = true;
    state = &http_client::connection_state_free;
    handler = http_client_handler_ptr();
    requests_processed = 0;
    if (buffer.size() == 0) {
        auto cfg = delegate->get_config();
        buffer.resize(cfg->io_buffer_size);
        request.resize(cfg->header_buffer_size);
        response.resize(cfg->header_buffer_size);
    } else {
#if ZERO_BUFFERS
        io_buffer::clear();
#endif
    }
    return true;
}

template <>
bool http_client_connection_tmpl<connection_tcp>::free(protocol_engine_delegate *delegate)
{
    state = &http_client::connection_state_free;
    handler = http_client_handler_ptr();
    return true;
}


/* http_client_config_factory */

void http_client_config_factory::make_config(config_ptr cfg) const
{
}


/* http_client */

const char* http_client::ClientName = "latypus";
const char* http_client::ClientVersion = "0.0.0";

std::once_flag http_client::protocol_init;

http_client::http_client(std::string name) : protocol(name) {}
http_client::~http_client() {}

protocol* http_client::get_proto()
{
    static http_client proto("http_client");
    return &proto;
}

void http_client::proto_init()
{
    std::call_once(protocol_init, [](){
        http_constants::init();
        protocol_engine::config_factory_map.insert
            (protocol_config_factory_entry(get_proto(), std::make_shared<http_client_config_factory>()));
    });
}

http_client_engine_state* http_client::get_engine_state(protocol_thread_delegate *delegate) {
    return static_cast<http_client_engine_state*>(delegate->get_engine_delegate()->get_engine_state(get_proto()));
}

http_client_engine_state* http_client::get_engine_state(protocol_engine_delegate *delegate) {
    return static_cast<http_client_engine_state*>(delegate->get_engine_state(get_proto()));
}

protocol_engine_state* http_client::create_engine_state() const
{
    return new http_client_engine_state();
}

protocol_thread_state* http_client::create_thread_state() const
{
    return new http_client_thread_state();
}

void http_client::engine_init(protocol_engine_delegate *delegate) const
{
    // get config
    auto cfg = delegate->get_config();
    
    // initialize connection table
    get_engine_state(delegate)->init(delegate, cfg->client_connections);
}

void http_client::engine_shutdown(protocol_engine_delegate *delegate) const
{
    
}

void http_client::thread_init(protocol_thread_delegate *delegate) const
{
    
}

void http_client::thread_shutdown(protocol_thread_delegate *delegate) const
{
    
}

void http_client::handle_message(protocol_thread_delegate *delegate, protocol_message &msg) const
{
    auto http_conn = get_connection(delegate, msg.connection_num);
    auto &conn = http_conn->conn;
    auto action = (*protocol_action::get_table())[msg.action];
    
    conn.set_last_activity(delegate->get_current_time());
    if (delegate->get_debug_mask() & protocol_debug_message) {
        log_debug("%90s:%p: %s: message: %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  http_conn->to_string().c_str(),
                  action->name.c_str());
    }
    action->callback(delegate, http_conn);
}

void http_client::handle_connection(protocol_thread_delegate *delegate, protocol_object *obj, int revents) const
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;
    time_t current_time = delegate->get_current_time();
    
    if (revents & poll_event_hup) {
        if (delegate->get_debug_mask() & protocol_debug_socket) {
            int socket_error = conn.get_sock_error();
            log_debug("%90s:%p: %s: %s",
                      delegate->get_thread_string().c_str(),
                      delegate->get_thread_id(),
                      obj->to_string().c_str(),
                      socket_error ? strerror(socket_error) : "connection closed");
        }
        delegate->remove_events(http_conn);
        close_connection(delegate, http_conn);
        return;
    } else if (revents & poll_event_err) {
        if (delegate->get_debug_mask() & protocol_debug_socket) {
            log_debug("%90s:%p: %s: socket exception",
                      delegate->get_thread_string().c_str(),
                      delegate->get_thread_id(),
                      obj->to_string().c_str());
        }
        delegate->remove_events(http_conn);
        close_connection(delegate, http_conn);
        return;
    } else if (revents & poll_event_invalid) {
        if (delegate->get_debug_mask() & protocol_debug_socket) {
            log_debug("%90s:%p: %s: invalid socket",
                      delegate->get_thread_string().c_str(),
                      delegate->get_thread_id(),
                      obj->to_string().c_str());
        }
        delegate->remove_events(http_conn);
        close_connection(delegate, http_conn);
    } else {
        conn.set_last_activity(current_time);
        if (http_conn->state->callback) {
            http_conn->state->callback(delegate, obj);
        } else {
            log_error("%90s:%p: %s: invalid connection state: state=%d",
                      delegate->get_thread_string().c_str(),
                      delegate->get_thread_id(),
                      obj->to_string().c_str(),
                      http_conn->state);
            delegate->remove_events(http_conn);
            abort_connection(delegate, http_conn);
            return;
        }
    }
}

void http_client::timeout_connection(protocol_thread_delegate *delegate, protocol_object *obj) const
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;
    auto cfg = delegate->get_config();
    time_t current_time = delegate->get_current_time();
    time_t last_activity = conn.get_last_activity();
    
    // timeout connections
    if (http_conn->state == &connection_state_free)
    {
        return;
    }
    else if (http_conn->state == &connection_state_client_request ||
             http_conn->state == &connection_state_client_body ||
             http_conn->state == &connection_state_server_response ||
             http_conn->state == &connection_state_server_body)
    {
        if (current_time - last_activity > cfg->connection_timeout) {
            if (delegate->get_debug_mask() & protocol_debug_timeout) {
                log_debug("%90s:%p: %s: inactivity timeout reached: aborting connection",
                          delegate->get_thread_string().c_str(),
                          delegate->get_thread_id(),
                          obj->to_string().c_str());
            }
            delegate->remove_events(http_conn);
            abort_connection(delegate, http_conn);
        }
    } else if (http_conn->state == &connection_state_waiting) {
        if (current_time - last_activity > cfg->keepalive_timeout) {
            if (delegate->get_debug_mask() & protocol_debug_timeout) {
                log_debug("%90s:%p: %s: keepalive timeout reached: closing connection",
                          delegate->get_thread_string().c_str(),
                          delegate->get_thread_id(),
                          obj->to_string().c_str());
            }
            delegate->remove_events(http_conn);
            close_connection(delegate, http_conn);
        }
    }
}


/* http_client state handlers */

void http_client::handle_state_client_request(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;
    auto &buffer = http_conn->buffer;

    // write request and request headers
    // TODO - if response has body then populate io_buffer and write in write_response_body
    io_result result = buffer.buffer_write(conn);
    if (result.has_error()) {
        log_error("%90s:%p: %s: write exception: aborting connection: %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  result.error_string().c_str());
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
        return;
    }
    
    // if there is any request header data still to be written then
    // enter poll loop waiting for another poll_event_out event
    if (buffer.bytes_readable() > 0) {
        return;
    }
    
    // clear buffers
    buffer.reset();
    
    // if request has a body then enter http_client_connection_state_client_body
    // otherwise enter http_client_connection_state_server_response to read response
    if (http_conn->request_has_body) {
        http_conn->state = &connection_state_client_body;
        get_proto()->handle_connection(delegate, obj, poll_event_in); // restart processing in the new state
    } else {
        delegate->remove_events(http_conn);
        delegate->add_events(http_conn, poll_event_in);
        http_conn->state = &connection_state_server_response;
    }
}

void http_client::handle_state_client_body(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);

    // write request body e.g. POST
    io_result result = http_conn->handler->write_request_body();
    if (result.has_error()) {
        log_error("%90s:%p: %s: handler write_request_body failed: aborting connection: %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  result.error_string().c_str());
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
    } else if (result.size() == 0) {
        delegate->remove_events(http_conn);
        delegate->add_events(http_conn, poll_event_in);
        http_conn->state = &connection_state_server_response;
    }
}

void http_client::handle_state_server_response(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;
    auto &buffer = http_conn->buffer;

    // read server response and response headers
    if (buffer.bytes_writable() <= 0) {
        log_error("%90s:%p: %s: header buffer full: aborting connection",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str());
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
        return;
    }
    io_result result = buffer.buffer_read(conn);
    if (result.has_error()) {
        log_error("%90s:%p: %s: read exception: aborting connection: %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  result.error_string().c_str());
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
        return;
    }

    // incrementally parse headers
#if USE_RINGBUFFER
    /* size_t bytes_parsed = */ http_conn->response.parse(buffer.data() + buffer.back - result.size(), result.size());
    buffer.front += result.size();
#else
    /* size_t bytes_parsed = */ http_conn->response.parse(buffer.data() + buffer.offset(), result.size());
    buffer.set_offset(buffer.offset() + result.size());
#endif
    
    // switch state if response processing is finished
    if (http_conn->response.is_finished()) {
        process_response_headers(delegate, http_conn);
        if (http_conn->response_has_body) {
            // copy body fragment to start of buffer
            // Note: body_start is stored in the io_buffer not the header_buffer so this results in a memmove
            buffer.set(http_conn->response.body_start.data, http_conn->response.body_start.length);
            http_conn->state = &connection_state_server_body;
            get_proto()->handle_connection(delegate, obj, poll_event_in); // restart processing in the new state
        } else {
            if (!http_conn->handler->end_request()) {
                log_error("%90s:%p: %s: handler end_request failed: aborting connection",
                          delegate->get_thread_string().c_str(),
                          delegate->get_thread_id(),
                          obj->to_string().c_str());
                delegate->remove_events(http_conn);
                abort_connection(delegate, http_conn);
            } else if (http_conn->connection_close) {
                if (delegate->get_debug_mask() & protocol_debug_socket) {
                    log_debug("%90s:%p: %s: closing connection",
                              delegate->get_thread_string().c_str(),
                              delegate->get_thread_id(),
                              obj->to_string().c_str());
                }
                delegate->remove_events(http_conn);
                close_connection(delegate, http_conn);
            } else {
                // remove last request from the connections request list
                delegate->remove_events(http_conn);
                http_conn->handler = http_client_handler_ptr();
                http_conn->requests_processed++;
                http_conn->connection_mutex.lock();
                http_conn->url_requests.pop_front();
                
                // check the request list for pending requests
                if (http_conn->url_requests.size() > 0) {
                    http_conn->connection_mutex.unlock();
                    process_connection(delegate, http_conn);
                } else {
                    http_conn->connection_mutex.unlock();
                    // TODO - consider keeping connection in keepalive state
#if 1
                    close_connection(delegate, http_conn);
#else
                    keepalive_connection(delegate, http_conn);
#endif
                }
            }
        }
    } else if (http_conn->response.has_error() || http_conn->response.is_finished()) {
        log_debug("%90s:%p: %s: header parse error: aborting connection",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str());
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
    }
}

void http_client::handle_state_server_body(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    
    // read server response and when finished close the connection
    // or forward the connection to the keepalive thread
    io_result result = http_conn->handler->read_response_body();
    if (result.has_error()) {
        log_error("%90s:%p: %s: handler read_request_body failed: aborting connection: %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  result.error_string().c_str());
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
    } else if (result.size() == 0) {
        if (!http_conn->handler->end_request()) {
            log_error("%90s:%p: %s: handler end_request failed: aborting connection",
                      delegate->get_thread_string().c_str(),
                      delegate->get_thread_id(),
                      obj->to_string().c_str());
            delegate->remove_events(http_conn);
            abort_connection(delegate, http_conn);
        } else if (http_conn->connection_close) {
            if (delegate->get_debug_mask() & protocol_debug_socket) {
                log_debug("%90s:%p: %s: closing connection",
                          delegate->get_thread_string().c_str(),
                          delegate->get_thread_id(),
                          obj->to_string().c_str());
            }
            delegate->remove_events(http_conn);
            close_connection(delegate, http_conn);
        } else {
            // remove last request from the connections request list
            delegate->remove_events(http_conn);
            http_conn->handler = http_client_handler_ptr();
            http_conn->requests_processed++;
            http_conn->connection_mutex.lock();
            http_conn->url_requests.pop_front();
            
            // check the request list for pending requests
            if (http_conn->url_requests.size() > 0) {
                http_conn->connection_mutex.unlock();
                process_connection(delegate, http_conn);
            } else {
                http_conn->connection_mutex.unlock();
                // TODO - consider keeping connection in keepalive state
#if 1
                close_connection(delegate, http_conn);
#else
                keepalive_connection(delegate, http_conn);
#endif
            }
        }
    }
}

void http_client::handle_state_waiting(protocol_thread_delegate *delegate, protocol_object *obj)
{
    // TODO - keepalive currently not reached
    // timed out on keepalive so we close the connection
    auto http_conn = static_cast<http_client_connection*>(obj);
    close_connection(delegate, http_conn);
}


/* http_client messages */

void http_client::connect_host(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;

    // connect to host
    http_conn->connection_mutex.lock();
    auto current_request = http_conn->url_requests.front();
    http_conn->connection_mutex.unlock();
    if (delegate->get_resolver()->lookup(conn.get_peer_addr(), current_request->url->host, current_request->url->port)) {
        if (conn.connect_to_host(conn.get_peer_addr())) {
            process_connection(delegate, http_conn);
        } else {
            abort_connection(delegate, http_conn);
        }
    } else {
        abort_connection(delegate, http_conn);
    }
}

void http_client::process_next_request(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;
    time_t current_time = delegate->get_current_time();
    
    // process the request at the head of this connections request queue
    socklen_t addrlen = sizeof(sockaddr_storage);
    if (getsockname(http_conn->get_poll_fd(), (sockaddr*)&conn.get_local_addr().storage, &addrlen) < 0) {
        log_debug("%90s:%p: %s: getsockname: %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  strerror(errno));
    }
    if (http_conn->requests_processed == 0 && delegate->get_debug_mask() & protocol_debug_socket) {
        log_debug("%90s:%p: %s: connected %s -> %s",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  socket_addr::addr_to_string(conn.get_local_addr()).c_str(),
                  socket_addr::addr_to_string(conn.get_peer_addr()).c_str());
    }
    http_conn->connection_mutex.lock();
    auto current_request = http_conn->url_requests.front();
    http_conn->connection_mutex.unlock();
    
    http_conn->handler = current_request->handler;
    http_conn->handler->init();
    http_conn->handler->set_delegate(delegate);
    http_conn->handler->set_connection(http_conn);
    http_conn->handler->set_current_time(current_time);
    
    http_conn->response.reset();
    http_conn->request.reset();
    http_conn->request.set_http_version(http_constants::get_version_text(HTTPVersion11));
    http_conn->request.set_request_method(http_constants::get_method_text(current_request->method));
    
    // TODO - handle canonical escaping of path
    http_conn->request.set_request_uri(current_request->url->path);
    http_conn->request.set_header_field(kHTTPHeaderHost, current_request->url->host);
    http_conn->request.set_header_field(kHTTPHeaderUserAgent, format_string("%s/%s", ClientName, ClientVersion));
    
    // TODO - handle option to close connection
    //http_serverrequest->set_header_field(kHTTPHeaderConnection, kHTTPTokenClose);
    if (populate_request_headers(delegate, http_conn) > 0) {
        http_conn->state = &connection_state_client_request;
        delegate->add_events(http_conn, poll_event_out);
    } else {
        log_debug("%90s:%p: %s: request buffer full",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str());
        abort_connection(delegate, http_conn);
    }
}

void http_client::keepalive_wait_connection(protocol_thread_delegate *delegate, protocol_object *obj)
{    
    if (delegate->get_debug_mask() & protocol_debug_event) {
        log_debug("%90s:%p: %s: connection keepalive",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str());
    }

    // TODO - consider waking up connections waiting in keepalive
    
    // keepalive state is not currently used by the client as
    // connections are closed if there are no pending url_requests
}


/* http_client internal */

ssize_t http_client::populate_request_headers(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &buffer = http_conn->buffer;
    
    if (!http_conn->handler->populate_request()) {
        abort_connection(delegate, http_conn);
    }
    
    // copy headers to io buffer
    buffer.reset();
    ssize_t length =  http_conn->request.to_buffer(buffer.data(), buffer.size());
    // check headers fit into available buffer space
    if (length < 0) {
        log_error("%90s:%p: %s: header buffer overflow",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str());
        // TODO - return error to client
        delegate->remove_events(http_conn);
        abort_connection(delegate, http_conn);
        return length;
    }
#if USE_RINGBUFFER
    buffer.back = length;
#else
    buffer.set_length(length);
#endif

    // debug request
    if (delegate->get_debug_mask() & protocol_debug_headers) {
        printf("%s", http_conn->request.to_string().c_str());
    }
    
    return length;
}

void http_client::process_response_headers(protocol_thread_delegate *delegate, protocol_object *obj)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    
    // TODO - check handler return code
    // TODO - handle redirect
    // TODO - handle authorization
    http_conn->handler->handle_response();
    
    // debug response
    if (delegate->get_debug_mask() & protocol_debug_headers) {
        printf("%s", http_conn->response.to_string().c_str());
    }
}

void http_client::connect_connection(protocol_thread_delegate *delegate, protocol_object *obj)
{
    forward_connection(delegate, obj, thread_mask_connect, action_connect_host);
}

void http_client::process_connection(protocol_thread_delegate *delegate, protocol_object *obj)
{
    forward_connection(delegate, obj, thread_mask_processor, action_process_next_request);
}

void http_client::keepalive_connection(protocol_thread_delegate *delegate, protocol_object *obj)
{
    forward_connection(delegate, obj, thread_mask_keepalive, action_keepalive_wait_connection);
}

void http_client::forward_connection(protocol_thread_delegate* delegate, protocol_object *obj,
                                              const protocol_mask &proto_mask, const protocol_action &proto_action)
{
    auto http_conn = static_cast<http_client_connection*>(obj);
    auto &conn = http_conn->conn;
    
    protocol_thread_delegate *destination_thread = delegate->choose_thread(proto_mask.mask);
    if (destination_thread) {
        delegate->send_message(destination_thread, protocol_message(proto_action.action, conn.get_id()));
    } else {
        log_error("%90s:%p: %s: no thread avaiable: %",
                  delegate->get_thread_string().c_str(),
                  delegate->get_thread_id(),
                  obj->to_string().c_str(),
                  proto_mask.name.c_str());
        abort_connection(delegate, http_conn);
    }
}

http_client_connection* http_client::get_connection(protocol_thread_delegate *delegate, int conn_id)
{
    return get_engine_state(delegate)->get_connection(delegate->get_engine_delegate(), conn_id);
}

void http_client::abort_connection(protocol_thread_delegate *delegate, protocol_object *obj)
{
    get_engine_state(delegate)->abort_connection(delegate->get_engine_delegate(), obj);
}

void http_client::close_connection(protocol_thread_delegate *delegate, protocol_object *obj)
{
    get_engine_state(delegate)->close_connection(delegate->get_engine_delegate(), obj);
}

http_client_connection* http_client::get_new_connection_for_url(protocol_engine_delegate *delegate, url_ptr url)
{
    http_client_engine_state *state = get_engine_state(delegate);
    http_client_connection *http_conn = nullptr;
    
    state->connections_mutex.lock();
    if (state->connections_free.size() > 0) {
        http_conn = state->connections_free.back();
        state->connections_free.pop_back();
    }
    if (http_conn) {
        // initialize connection
        http_conn->init(delegate);
        // set host on connection
        http_conn->remote_host = url->host;
        // find connection list in host map
        auto cli = state->connections_host_map.find(url->host);
        if (cli == state->connections_host_map.end()) {
            // add connection to a new connection list
            state->connections_host_map.insert(http_client_engine_state::connection_host_entry(url->host, http_client_engine_state::connection_queue{http_conn}));
        } else {
            // add connection to the existing connection list
            (*cli).second.push_back(http_conn);
        }
    }
    state->connections_mutex.unlock();
    return http_conn;
}

http_client_connection* http_client::get_existing_connection_for_url(protocol_engine_delegate *delegate, url_ptr url, size_t max_requests_per_connection)
{
    http_client_engine_state *state = get_engine_state(delegate);
    http_client_connection *http_conn = nullptr;
    
    state->connections_mutex.lock();
    auto cli = state->connections_host_map.find(url->host);
    if (cli != state->connections_host_map.end()) {
        auto &connection_queue = (*cli).second;
        auto ci = std::find_if(connection_queue.begin(), connection_queue.end(), [=](http_client_connection *c) {
            //c->connection_mutex.lock();
            size_t outstanding_requests = c->url_requests.size(); // TODO - check atomicity?
            //c->connection_mutex.unlock();
            return outstanding_requests < max_requests_per_connection;
        });
        
        if (ci != connection_queue.end()) {
            http_conn = *ci;
            if (connection_queue.size() > 1) {
                connection_queue.push_back(connection_queue.front());
                connection_queue.pop_front();
            }
        }
    }
    state->connections_mutex.unlock();
    return http_conn;
}


/* public interface */

bool http_client::submit_request(protocol_engine_delegate *delegate,
                                          http_client_request_ptr url_req,
                                          size_t max_requests_per_connection)
{
    http_client_connection *http_conn = nullptr;
    
    if (max_requests_per_connection > 0) {
        http_conn = get_existing_connection_for_url(delegate, url_req->url, max_requests_per_connection);
    }
    if (http_conn) {
        // make sure the connection still has an active url_requests list as the last request
        // may been finished and the connection closed inbetween the time we found it and now
        // TODO - consider waking up connections waiting in a keepalive state
        http_conn->connection_mutex.lock();
        if (http_conn->url_requests.size() > 0) {
            http_conn->url_requests.push_back(url_req);
            http_conn->connection_mutex.unlock();
            return true;
        } else {
            http_conn->connection_mutex.unlock();
        }
    }
    http_conn = get_new_connection_for_url(delegate, url_req->url);
    if (http_conn) {
        http_conn->connection_mutex.lock();
        http_conn->url_requests.push_back(url_req);
        http_conn->connection_mutex.unlock();
        protocol_thread_delegate *thread_delegate = delegate->choose_thread(thread_mask_connect.mask);
        forward_connection(thread_delegate, http_conn, thread_mask_connect, action_connect_host);
        return true;
    } else {
        log_error("submit_request: no available connections");
        return false;
    }
}
