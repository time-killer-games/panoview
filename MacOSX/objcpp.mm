/*

 MIT License

 Copyright © 2021 Samuel Venable

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#include <string>

#include "objcpp.h"
#include "../Universal/crossprocess.h"

bool cocoa_pid_from_wid(CGWindowID wid, pid_t *pid, bool onscreenonly) {
  bool result = false; CGWindowListOption options = kCGWindowListOptionIncludingWindow | 
  kCGWindowListExcludeDesktopElements | (onscreenonly ? kCGWindowListOptionOnScreenOnly : 0);
  const CGWindowLevel kScreensaverWindowLevel = CGWindowLevelForKey(kCGScreenSaverWindowLevelKey);
  CFArrayRef windowArray = CGWindowListCopyWindowInfo(options, wid);
  CFIndex windowCount = 0;
  if ((windowCount = CFArrayGetCount(windowArray))) {
    for (CFIndex i = 0; i < windowCount; i++) {
      NSDictionary *windowInfoDictionary =
      (__bridge NSDictionary *)((CFDictionaryRef)CFArrayGetValueAtIndex(windowArray, i));
      NSNumber *ownerPID = (NSNumber *)(windowInfoDictionary[(id)kCGWindowOwnerPID]);
      NSNumber *level = (NSNumber *)(windowInfoDictionary[(id)kCGWindowLayer]);
      if (level.integerValue < kScreensaverWindowLevel) {
        NSNumber *windowID = windowInfoDictionary[(id)kCGWindowNumber];
        if (wid == windowID.integerValue) {
          *pid = ownerPID.integerValue;
          result = true;
          break;
        }
      }
    }
  }
  CFRelease(windowArray);
  return result;
}

void window_get_rect_from_id(char *window, int *x, int *y, int *width, int *height) {
  CFArrayRef windowArray = CGWindowListCopyWindowInfo(
  kCGWindowListOptionAll, kCGNullWindowID);
  CFIndex windowCount = 0;
  if ((windowCount = CFArrayGetCount(windowArray))) {
    for (CFIndex j = 0; j < windowCount; j++) {
      CFDictionaryRef windowInfoDictionary = 
      (CFDictionaryRef)CFArrayGetValueAtIndex(windowArray, j);
      CFNumberRef windowID = (CFNumberRef)CFDictionaryGetValue(
      windowInfoDictionary, kCGWindowNumber);
      CGWindowID wid; CFNumberGetValue(windowID,
      kCGWindowIDCFNumberType, &wid);
      if (strtoull(window, nullptr, 10) == wid) {
        CGRect rect; CFDictionaryRef dict = (CFDictionaryRef)CFDictionaryGetValue(
        windowInfoDictionary, kCGWindowBounds);
        CGRectMakeWithDictionaryRepresentation(dict, &rect);
        *x = rect.origin.x;
        *y = rect.origin.y;  
        *width = CGRectGetWidth(rect);
        *height = CGRectGetHeight(rect);    
      }
    }
  }
  CFRelease(windowArray);
}

void setpolicy() {
  if (CrossProcess::WindowIdExists((CrossProcess::WINDOWID)
    CrossProcess::EnvironmentGetVariable("WINDOWID"))) {
    [[NSApplication sharedApplication] setActivationPolicy:(NSApplicationActivationPolicy)1];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
  }
  CrossProcess::WINDOWID *wid = nullptr; int widsize = 0;
  CrossProcess::WindowIdFromProcId(getpid(), &wid, &widsize);
  if (wid) { 
    if (widsize) { 
      for (int i = 0; i < widsize; i++) { 
        const char *title = window_id_set_parent_window_id(wid[i], (char *)"0");
        if (title != nullptr && strcmp(title, "") == 0) {
          int hdw = CGDisplayPixelsWide(kCGDirectMainDisplay) / 2;
          int hdh = CGDisplayPixelsHigh(kCGDirectMainDisplay) / 2;
          [[NSApp windowWithWindowNumber:strtoull(wid[i], nullptr, 10)] 
          setFrame:NSMakeRect(hdw - 320, hdh - 240, 640, 480) display:YES];
        } 
      }
    }
    CrossProcess::FreeWindowId(wid);
  } 
}

char *window_id_from_native_window(char *native) {
  static std::string window; 
  window = std::to_string([(NSWindow *)(void *)strtoull(native, nullptr, 10) windowNumber]);
  return (char *)window.c_str();
}

char *native_window_from_window_id(char *window) {
  static std::string native; 
  native = std::to_string((unsigned long long)(void *)[NSApp windowWithWindowNumber:strtoull(window, nullptr, 10)]);
  return (char *)native.c_str();
}

int window_get_x_from_id(char *window) {
  int x = 0, y = 0, width = 0, height = 0;
  window_get_rect_from_id(window, &x, &y, &width, &height);
  return x;
}

int window_get_y_from_id(char *window) {
  int x = 0, y = 0, width = 0, height = 0;
  window_get_rect_from_id(window, &x, &y, &width, &height);
  return y;
}

int window_get_width_from_id(char *window) {
  int x = 0, y = 0, width = 0, height = 0;
  window_get_rect_from_id(window, &x, &y, &width, &height);
  return width;
}

int window_get_height_from_id(char *window) {
  int x = 0, y = 0, width = 0, height = 0;
  window_get_rect_from_id(window, &x, &y, &width, &height);
  return height;
}

static int titlebar_height = 0; 
const char *window_id_set_parent_window_id(char *wid, char *pwid) {
  char *native = native_window_from_window_id(wid);
  NSWindow *window = (NSWindow *)(void *)strtoull(native, nullptr, 10);
  if (strcmp(pwid, "0") != 0) {
    if (titlebar_height == 0) titlebar_height = window.frame.size.height - 
    window.contentView.frame.size.height;
    int x = 0, y = 0, width = 0, height = 0;
    window_get_rect_from_id(pwid, &x, &y, &width, &height);
    [window setFrame:NSMakeRect(x, y + titlebar_height, width, height - titlebar_height) display:YES];
    [window orderWindow:NSWindowAbove relativeTo:strtoull(pwid, nullptr, 10)];
    [window setStyleMask:NSWindowStyleMaskBorderless]; pid_t pid; 
    [window setIsVisible:(BOOL)cocoa_pid_from_wid(strtoull(pwid, nullptr, 10), &pid, true)];
    if (kill(pid, 0) != 0) exit(0);
  }
  [window setTitleVisibility:NSWindowTitleHidden];
  return [[window title] UTF8String];
}
