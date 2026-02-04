#include "ticker.h"

#include <cassert>

#include "my_logger.h"

namespace ticker {

Ticker::Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
    : strand_(std::move(strand))
    , period_(period)
    , handler_(std::move(handler))
    , timer_(strand_)  // initialize timer with strand
{}

void Ticker::Start() {
    last_tick_ = Clock::now();
    net::dispatch(strand_, [self = shared_from_this()] { self->ScheduleTick(); });
}

void Ticker::ScheduleTick() {
    assert(strand_.running_in_this_thread());
    timer_.expires_after(period_);
    timer_.async_wait([self = shared_from_this()](sys::error_code ec) { self->OnTick(ec); });
}

void Ticker::OnTick(sys::error_code ec) {
    using namespace std::chrono;
    assert(strand_.running_in_this_thread());

    if (!ec) {
        auto this_tick = Clock::now();
        auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
        last_tick_ = this_tick;
        try {
            handler_(delta);
        } catch (...) {
            // swallow exceptions to keep ticker running
        }
        ScheduleTick();
    } else {
        if (ec != net::error::operation_aborted) {
            logger::LogNetError(ec.value(), ec.message(), "Ticker::OnTick");
        }
    }
}

}  // namespace ticker
