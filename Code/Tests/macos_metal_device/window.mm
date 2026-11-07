// [TEMP] [CHOSEN, not recovered] Hidden real Cocoa window, no engine/input mocks.
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
struct TestWindow { NSWindow* window; CAMetalLayer* layer; };
extern "C" void* metalTestWindow()
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        [NSApp finishLaunching];
        auto* t = new TestWindow;
        t->window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,320,240)
            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
        t->window.releasedWhenClosed = NO;
        t->layer = [CAMetalLayer layer];
        t->layer.drawableSize = CGSizeMake(320,240);
        t->window.contentView.wantsLayer = YES;
        t->window.contentView.layer = t->layer;
        [t->window orderOut:nil];
        return t;
    }
}
extern "C" void* metalTestView(void* handle)
{ return (__bridge void*)static_cast<TestWindow*>(handle)->window.contentView; }
extern "C" void metalTestResize(void* handle)
{
    auto* t = static_cast<TestWindow*>(handle);
    [t->window setContentSize:NSMakeSize(400,300)];
    t->layer.drawableSize = CGSizeMake(400,300);
}
// The VideoOptions resolution change: the host resizes the window and the drawable first.
extern "C" void metalTestResizeTo(void* handle, int width, int height)
{
    auto* t = static_cast<TestWindow*>(handle);
    [t->window setContentSize:NSMakeSize(width,height)];
    t->layer.drawableSize = CGSizeMake(width,height);
}
extern "C" void metalTestDrawableAvailable(void* handle, bool available)
{
    // CAMetalLayer ignores a zero drawableSize, so simulate an unrenderable window
    // through the content view's bounds, which the backend also checks.
    auto* t = static_cast<TestWindow*>(handle);
    [t->window.contentView setFrameSize:available ? NSMakeSize(320,240) : NSZeroSize];
    if (available) t->layer.drawableSize = CGSizeMake(320,240);
}
extern "C" void metalTestDestroy(void* handle)
{
    @autoreleasepool {
        auto* t = static_cast<TestWindow*>(handle);
        [t->window close];
        delete t;
    }
}
