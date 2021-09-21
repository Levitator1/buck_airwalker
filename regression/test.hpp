#pragma once
#include <string>

//Says something...
//and then commit to saying "FAILED" unless notified to say "OK"
class EllipsisGuard{
    bool m_ok = false;
public:
    EllipsisGuard(const std::string &msg);
    ~EllipsisGuard();
    void ok();
};

