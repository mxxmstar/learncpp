#pragma once
#include <cstdint>
#include <string>

// 通用错误码定义 (格式: 0xTMMMS, T=Type, MMM=Module, SS=Specific Code)
namespace ErrorCode {
namespace bits {
    /// @brief 错误码类型位 4位
    static constexpr uint32_t type_mask = 0xF0000000U;
    static constexpr uint32_t type_shift = 28;

    /// @brief 错误码模块位 12位
    static constexpr uint32_t module_mask = 0x0FFF0000U;
    static constexpr uint32_t module_shift = 16;

    /// @brief 错误码具体码位 16位，高8位保留
    static constexpr uint32_t specific_code_mask = 0x0000FFFFU;
    static constexpr uint32_t specific_code_shift = 0;
}

/// @brief 错误码类型
struct Type {
    static constexpr uint8_t Success     = 0x0;
    static constexpr uint8_t Info        = 0x1;
    static constexpr uint8_t Warning     = 0x2;
    static constexpr uint8_t Error       = 0x3;
    static constexpr uint8_t Critical    = 0x4;
    static constexpr uint8_t Fatal       = 0x5;
};

///@brief 模块分类定义
struct Module {
    static constexpr uint16_t Common      = 0x000;  // 通用处理模块-进程
    static constexpr uint16_t Net         = 0x100;  // 网络模块
    static constexpr uint16_t Media       = 0x200;  // 媒体处理模块
    static constexpr uint16_t File        = 0x300;  // 文件操作模块
    static constexpr uint16_t Protocol    = 0x400;  // 协议处理模块
    static constexpr uint16_t Auth        = 0x500;  // 认证授权模块
    static constexpr uint16_t DB          = 0x600;  // 数据库模块
    static constexpr uint16_t Recommend   = 0x700;  // 推荐模块
    static constexpr uint16_t Notify      = 0x800;  // 通知模块
};

namespace Common {
    static constexpr uint16_t Process = 0x001;  // 通用处理模块-进程

    // #define ERROR_INVALID_PARAM             0x00010001  // 参数无效
// #define ERROR_OUT_OF_MEMORY             0x03010002  // 内存不足
// #define ERROR_TIMEOUT                   0x03010003  // 操作超时
// #define ERROR_NOT_FOUND                 0x03010004  // 资源未找到
// #define ERROR_ACCESS_DENIED             0x03010005  // 访问被拒绝
// #define ERROR_NOT_IMPLEMENTED           0x03010006  // 功能未实现
// #define ERROR_BUFFER_OVERFLOW           0x03010007  // 缓冲区溢出
// #define ERROR_INVALID_STATE             0x03010008  // 状态无效
// #define ERROR_CONNECTION_FAILED         0x03010009  // 连接失败
// #define ERROR_DISCONNECTED              0x0301000A  // 已断开连接
// #define ERROR_ALREADY_EXISTS            0x0301000B  // 已存在
// #define ERROR_PERMISSION_DENIED         0x0301000C  // 权限不足
}

namespace Net {
    // #define NET_ERROR_SOCKET_CREATE         0x03010101  // Socket创建失败
// #define NET_ERROR_BIND_FAILED           0x03010102  // 绑定失败
// #define NET_ERROR_LISTEN_FAILED         0x03010103  // 监听失败
// #define NET_ERROR_ACCEPT_FAILED         0x03010104  // 接受连接失败
// #define NET_ERROR_CONNECT_FAILED        0x03010105  // 连接失败
// #define NET_ERROR_SEND_FAILED           0x03010106  // 发送数据失败
// #define NET_ERROR_RECV_FAILED           0x03010107  // 接收数据失败
// #define NET_ERROR_SSL_HANDSHAKE_FAILED  0x03010108  // SSL握手失败
// #define NET_ERROR_HTTP_PARSE_FAILED     0x03010109  // HTTP解析失败
// #define NET_ERROR_HTTP_PARSE_JSON_FAILED   0x0301010A   // HTTP解析JSON失败
    namespace Http {
        static constexpr uint16_t HttpModule = 0x101;  // HTTP模块标识
        static constexpr uint32_t NetworkError = 0x0001;  // 网络错误模块标识
        static constexpr uint32_t ResolveFailed       = 0x0002; // 解析域名失败
        static constexpr uint32_t ConnectFailed      = 0x0003;  // 连接失败
        static constexpr uint32_t ConnectTimeout     = 0x0004;  // 连接超时
        static constexpr uint32_t WriteFailed        = 0x0004;  // HTTP写入失败
        static constexpr uint32_t ReadFailed          = 0x0005; // HTTP读取失败

        static constexpr uint32_t InvalidRequest      = 0x0011;  // HTTP请求无效
        static constexpr uint32_t InvalidResponse     = 0x0012;  // HTTP响应无效
        
        
        static constexpr uint32_t ParseJsonFailed    = 0x0030;  // HTTP解析JSON失败
        static constexpr uint32_t MissingJsonField   = 0x0031;  // HTTP请求体中缺少字段
    }
}

namespace Media {
    // #define MEDIA_ERROR_INIT_FAILED         0x03010201  // 媒体初始化失败
// #define MEDIA_ERROR_CODEC_NOT_FOUND     0x03010202  // 编解码器未找到
// #define MEDIA_ERROR_STREAM_OPEN_FAILED  0x03010203  // 流打开失败
// #define MEDIA_ERROR_FORMAT_UNSUPPORTED  0x03010204  // 格式不支持
// #define MEDIA_ERROR_DECODE_FAILED       0x03010205  // 解码失败
// #define MEDIA_ERROR_ENCODE_FAILED       0x03010206  // 编码失败
// #define MEDIA_ERROR_RTSP_SETUP_FAILED   0x03010207  // RTSP设置失败
    namespace Zlmediakit {
        static constexpr uint16_t ZlmModule = 0x201;  // ZLMediaKit模块标识
        //static constexpr uint16_t NoError = 0x0000; // ZLM无错误
        static constexpr uint32_t UnknowError  = 0x0001;  // 未知错误
        static constexpr uint32_t InternalError     = 0x0002;   // 内部错误
        static constexpr uint32_t HookSecretMismatch = 0x0003;  // ZLM Hook密钥不匹配
        static constexpr uint32_t HookRequestInvalid = 0x0004;  // ZLM Hook请求无效
        static constexpr uint32_t HookEventUnknown   = 0x0005;  // ZLM未知事件类型
        static constexpr uint32_t StreamPublishDeny  = 0x0006;  // ZLM流发布被拒绝
        static constexpr uint32_t StreamPlayDeny     = 0x0007;  // ZLM流播放被拒绝        
    }
}

// 文件模块错误码 (MODULE_FILE = 0x03)
// #define FILE_ERROR_OPEN_FAILED          0x03010301  // 文件打开失败
// #define FILE_ERROR_READ_FAILED          0x03010302  // 文件读取失败
// #define FILE_ERROR_WRITE_FAILED         0x03010303  // 文件写入失败
// #define FILE_ERROR_NOT_EXIST            0x03010304  // 文件不存在
// #define FILE_ERROR_PATH_TOO_LONG        0x03010305  // 路径过长
// #define FILE_ERROR_DISK_FULL            0x03010306  // 磁盘空间不足

// 协议模块错误码 (MODULE_PROTOCOL = 0x04)
// #define PROTO_ERROR_DETECTION_FAILED    0x03010401  // 协议检测失败
// #define PROTO_ERROR_UNSUPPORT           0x03010402  // 协议不支持
// #define PROTO_ERROR_PARSE_FAILED        0x03010403  // 协议解析失败
// #define PROTO_ERROR_VERSION_MISMATCH    0x03010404  // 协议版本不匹配

// 认证模块错误码 (MODULE_AUTH = 0x05)
// #define AUTH_ERROR_INVALID_CREDENTIAL   0x03010501  // 凭据无效
// #define AUTH_ERROR_EXPIRED              0x03010502  // 凭据已过期
// #define AUTH_ERROR_TOKEN_INVALID        0x03010503  // Token无效
// #define AUTH_ERROR_NO_PERMISSION        0x03010504  // 无权限

/// 辅助函数定义
// 提取错误码各部分的函数
constexpr uint8_t ErrorType(uint32_t code) {
    return static_cast<uint8_t>((code & bits::type_mask) >> bits::type_shift);
}

constexpr uint16_t ModuleId(uint32_t code) {
    return static_cast<uint16_t>((code & bits::module_mask) >> bits::module_shift);
}

constexpr uint16_t SpecificCode(uint32_t code) {
    return static_cast<uint16_t>((code & bits::specific_code_mask) >> bits::specific_code_shift);
}

/// @brief 创建错误码
/// @param err_type 错误类型
/// @param module_id 模块id
/// @param code 具体错误码
/// @return 32位错误码
constexpr uint32_t MakeErrorCode(uint8_t err_type, uint16_t module_id, uint16_t code) {
    return ((static_cast<uint32_t>(err_type) << bits::type_shift) & bits::type_mask) |
           ((static_cast<uint32_t>(module_id) << bits::module_shift) & bits::module_mask) |
           (static_cast<uint16_t>((code & bits::specific_code_mask) >> bits::specific_code_shift));
}

constexpr uint32_t MakeZlmErrorCode(uint16_t code) {
    return MakeErrorCode(Type::Error, Media::Zlmediakit::ZlmModule, code);
}

constexpr uint32_t MakeZlmSuccessCode(uint16_t code = static_cast<uint16_t>(0)) {
    return MakeErrorCode(Type::Success, Media::Zlmediakit::ZlmModule, code);
}

constexpr uint32_t MakeHttpErrorCode(uint16_t code) {
    return MakeErrorCode(Type::Error, Net::Http::HttpModule, code);
}



}