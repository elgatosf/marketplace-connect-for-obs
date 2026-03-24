#include "util-macos.h"
#include "util.h"

#import <AppKit/AppKit.h>

bool isProtocolHandlerRegisteredMacOS(const std::wstring& protocol)
{
    std::string utf8(protocol.begin(), protocol.end());
    NSString* scheme =
        [NSString stringWithUTF8String:utf8.c_str()];

    if (!scheme) {
        return false;
    }

    NSURL* testURL =
        [NSURL URLWithString:
            [NSString stringWithFormat:@"%@://test", scheme]];

    if (!testURL) {
        return false;
    }

    NSURL* appURL =
        [[NSWorkspace sharedWorkspace]
            URLForApplicationToOpenURL:testURL];

    return (appURL != nil);
}

StreamDeckInfo getStreamDeckInfoMacOS()
{
    StreamDeckInfo info{ false, "" };

    // Locate Stream Deck via LaunchServices
    NSURL* appURL =
        [[NSWorkspace sharedWorkspace]
            URLForApplicationWithBundleIdentifier:@"com.elgato.StreamDeck"];

    if (!appURL) {
        return info; // Not installed
    }

    NSBundle* bundle = [NSBundle bundleWithURL:appURL];
    if (!bundle) {
        return info;
    }

    NSString* version =
        bundle.infoDictionary[@"CFBundleShortVersionString"];

    if (!version) {
        version = bundle.infoDictionary[@"CFBundleVersion"];
    }

    if (version) {
        info.installed = true;
        info.version = version.UTF8String;
    }

    return info;
}
