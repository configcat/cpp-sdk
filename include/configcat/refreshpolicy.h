#pragma once

namespace configcat {

class RefreshPolicy {
    virtual Config getConfiguration() = 0;
    virtual void close() = 0;
    virtual void refresh() = 0;

    virtual ~RefreshPolicy();
}

class DefaultRefreshPolicy : public RefreshPolicy {

};


} // namespace configcat
