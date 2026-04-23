#pragma once
#include <vector>
#include <memory>
#include <boost/asio.hpp>
struct SendBuffer {
    std::shared_ptr<std::vector<uint8_t>> data;
    std::size_t offset = 0;

    SendBuffer(std::shared_ptr<std::vector<uint8_t>> buf)
        : data(std::move(buf)), offset(0)
    {
    }

    boost::asio::const_buffer GetAsioConstBuffer() const {
        if (data && offset < data->size()) {
            return boost::asio::buffer(data->data() + offset, data->size() - offset);
        }
        return boost::asio::const_buffer();
    }

    bool IsComplete() const {
        return !data || offset >= data->size();
    }

    void UpdateOffset(std::size_t size) {
        offset += size;
    }

};
