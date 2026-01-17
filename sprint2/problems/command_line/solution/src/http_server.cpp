#include "http_server.h"

#include <boost/asio/dispatch.hpp>

namespace http_server {

void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(), beast::bind_front_handler(&SessionBase::Read, shared_from_this()));
}

}  // namespace http_server
