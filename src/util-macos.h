#pragma once

#include <string>

struct StreamDeckInfo;

// macOS-only implementation
bool isProtocolHandlerRegisteredMacOS(const std::wstring& protocol);
StreamDeckInfo getStreamDeckInfoMacOS();
