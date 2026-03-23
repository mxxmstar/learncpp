#include <iostream>
#include <cstdlib>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main() {
    std::cout << "Testing FFmpeg library linking..." << std::endl;
    
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "FFmpeg configuration: " << avutil_configuration() << std::endl;
    std::cout << "FFmpeg license: " << avutil_license() << std::endl;
    
    unsigned version = avformat_version();
    std::cout << "libavformat version: " << AV_VERSION_MAJOR(version) << "." 
              << AV_VERSION_MINOR(version) << "." 
              << AV_VERSION_MICRO(version) << std::endl;
    
    unsigned codec_version = avcodec_version();
    std::cout << "libavcodec version: " << AV_VERSION_MAJOR(codec_version) << "." 
              << AV_VERSION_MINOR(codec_version) << "." 
              << AV_VERSION_MICRO(codec_version) << std::endl;
    
    unsigned util_version = avutil_version();
    std::cout << "libavutil version: " << AV_VERSION_MAJOR(util_version) << "." 
              << AV_VERSION_MINOR(util_version) << "." 
              << AV_VERSION_MICRO(util_version) << std::endl;
    
    std::cout << "\nFFmpeg library linking test: SUCCESS" << std::endl;
    return 0;
}
