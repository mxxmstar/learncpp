# 批量更新头文件引用路径脚本

$rootPath = "d:\file_mx\aaaaa\learncpp"

# 定义替换规则
$replacements = @{
    '#include "net/httpclient.h"' = '#include "net/http_client/http_client.h"';
    '#include "net/httpclientpool.h"' = '#include "net/http_client/http_client_pool.h"';
    '#include "net/httpserver.h"' = '#include "net/http_server/http_server.h"';
    '#include "net/httprouter.h"' = '#include "net/http_server/http_router.h"';
    '#include "net/httpsession.h"' = '#include "net/http_server/http_session.h"';
    '#include "net/tcpserver.h"' = '#include "net/tcp_server/tcpserver.h"';
    '#include "net/tcpsession.h"' = '#include "net/tcp_server/tcpsession.h"';
    '#include "net/session.h"' = '#include "net/tcp_server/session.h"';
    '#include "net/databuffer.h"' = '#include "net/tcp_server/databuffer.h"';
    '#include "net/websocket.h"' = '#include "net/websocket/websocket.h"';
    '#include "net/websocket_router.h"' = '#include "net/websocket/websocket_router.h"';
    '#include "net/websocket_server.h"' = '#include "net/websocket/websocket_server.h"';
    '#include "net/websocket_session.h"' = '#include "net/websocket/websocket_session.h"';
    '#include "net/asio_io_context_pool.h"' = '#include "net/io_context_pool/asio_io_context_pool.h"';
}

# 查找所有 .h 和 .cpp 文件
$files = Get-ChildItem -Path $rootPath -Include *.h,*.cpp -Recurse -File | 
    Where-Object { $_.FullName -notmatch '\\out\\|\\.git\\|vcpkg\\' }

Write-Host "找到 $($files.Count) 个文件，开始更新..."

$updatedCount = 0
foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw -Encoding UTF8
    $originalContent = $content
    
    foreach ($key in $replacements.Keys) {
        if ($content -match [regex]::Escape($key)) {
            $content = $content -replace [regex]::Escape($key), $replacements[$key]
        }
    }
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -NoNewline
        $updatedCount++
        Write-Host "  Updated: $($file.FullName.Substring($rootPath.Length + 1))"
    }
}

Write-Host "`nDone! Updated $updatedCount files"
