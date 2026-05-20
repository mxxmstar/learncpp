#ifndef MEDIA_EXPORT_H
#define MEDIA_EXPORT_H

/**
 * @file media_export.h
 * @brief DLL 导出宏定义
 *
 * 用于控制符号的可见性。
 * Windows: 使用 __declspec(dllexport/dllimport)
 * Linux/macOS: 使用 __attribute__((visibility("default")))
 */

#ifdef _WIN32
    #ifdef MEDIA_EXPORTS
        #define MEDIA_API __declspec(dllexport)
    #else
        #define MEDIA_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define MEDIA_API __attribute__((visibility("default")))
#else
    #define MEDIA_API
#endif

#endif /* MEDIA_EXPORT_H */
