#pragma once

#include <auth/time/IClock.hpp>

class FakeClock : public auth::IClock
{
    public:
        FakeClock():
            _now(std::chrono::steady_clock::now())
        {}
        std::chrono::steady_clock::time_point   now() const override
        {
            return this->_now;
        }
        void    advance(std::chrono::seconds duration)
        {
            this->_now += duration;
        }
    private:
        std::chrono::steady_clock::time_point   _now;
};
