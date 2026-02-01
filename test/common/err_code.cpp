#include "common/errcode.h"
#include <iostream>
#include <format>

using namespace ErrorCode::Net::Http;
int main() {
    std::cout << std::format("{:#x}\n", ErrorCode::MakeHttpErrorCode(InvalidRequest));

}