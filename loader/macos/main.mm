#import <Cocoa/Cocoa.h>
#include <string>
#include "urlHandler.hpp"

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    // If launched normally (not via URL), just quit.
    // You can change this behavior if you want it to stay running.
    [NSApp terminate:nil];
}

- (void)application:(NSApplication *)application openURLs:(NSArray<NSURL *> *)urls
{
    redirect_stdio_to_file();
    for (NSURL* u in urls)
    {
        if (!u) continue;

        std::string urlStr([[u absoluteString] UTF8String]);
        UrlHandler::handleUrl(urlStr);
    }

    // Quit after handling
    [NSApp terminate:nil];
}

@end

int main(int argc, const char * argv[])
{
    @autoreleasepool
    {
        NSApplication* app = [NSApplication sharedApplication];
        AppDelegate* delegate = [AppDelegate new];
        [app setDelegate:delegate];
        return NSApplicationMain(argc, argv);
    }
}
