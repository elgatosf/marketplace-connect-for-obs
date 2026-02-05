#pragma once
#include <string>

void redirect_stdio_to_file();

namespace UrlHandler
{
    // Called when the app is launched via URL scheme, e.g. elgatolink://foo
    int handleUrl(const std::string& url);
}
