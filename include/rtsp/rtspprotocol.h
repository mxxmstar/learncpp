#pragma once

namespace rtsp {


enum class RtspMethod {
    OPTIONS = 0,
    DESCRIBE,
    SETUP,
    PLAY,
    PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    RECORD,
    RTCP,
    UNKNOWN = 127,
};

// enum class TransportMode

enum class ConnectionState {
    kConnected,
    kOptions,
    kDescribe,
    kSetup,
    kPlay,
    kRecord,
    kStreaming
};

}