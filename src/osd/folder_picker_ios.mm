// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
// A Sega Model 1, 2 & 3 arcade emulator.
// Copyright (c) 2025+ Daniel Martin (dmanlfc)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This header must not be removed. Keisho is licensed under the GNU General
// Public License, version 3 or later, with additional terms under section 7 of
// that licence. See LICENSE and NOTICE.
//
// iOS: folders are chosen with the Files app's picker. A folder outside the
// app's own container is only reachable through the URL the picker returns,
// and only while its security scope is open, so each pick is kept open for the
// session and saved as a bookmark (in the app's user defaults) that the next
// launch resolves again.

#include "osd/folder_picker.h"

#include "core/config.h"
#include "core/log.h"

#include <SDL3/SDL.h>

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <array>
#include <utility>

@interface SM2FolderPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property(nonatomic, copy) NSString* key;
@property(nonatomic, assign) std::function<void(std::string)>* done;
@end

namespace {

/// The delegate of the picker on screen; the picker holds its delegate weakly.
SM2FolderPickerDelegate* g_delegate = nil;

NSString* defaults_key(NSString* key)
{
    return [@"keisho.folder." stringByAppendingString:key];
}

/// Open `url`'s security scope for the rest of the session. A folder inside
/// the app's own container needs none, and the call then returns NO, harmlessly.
void open_scope(NSURL* url)
{
    [url startAccessingSecurityScopedResource];
}

void remember(NSURL* url, NSString* key)
{
    NSError* error    = nil;
    NSData*  bookmark = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil
                                               error:&error];
    if (bookmark == nil) {
        SM2_WARN("folder picker: could not remember %s: %s", url.path.UTF8String,
                 error.localizedDescription.UTF8String);
        return;
    }
    [[NSUserDefaults standardUserDefaults] setObject:bookmark forKey:defaults_key(key)];
}

void finish(SM2FolderPickerDelegate* delegate, std::string path)
{
    std::function<void(std::string)>* done = delegate.done;
    delegate.done                          = nullptr;
    if (g_delegate == delegate) g_delegate = nil;
    if (done != nullptr) {
        (*done)(std::move(path));
        delete done;
    }
}

}  // namespace

@implementation SM2FolderPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    NSURL* url = urls.firstObject;
    if (url == nil) {
        finish(self, std::string());
        return;
    }
    open_scope(url);
    remember(url, self.key);
    finish(self, std::string(url.path.UTF8String));
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
    finish(self, std::string());
}

@end

namespace sm2::osd {

bool native_folder_picker_available()
{
    return true;
}

void pick_folder(SDL_Window* window, const std::string& key, std::function<void(std::string)> done)
{
    @autoreleasepool {
        UIWindow* ui_window = (__bridge UIWindow*)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
        UIViewController* host = ui_window.rootViewController;
        while (host.presentedViewController != nil) host = host.presentedViewController;
        if (host == nil || g_delegate != nil) {
            if (done) done(std::string());
            return;
        }

        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeFolder ]];
        picker.allowsMultipleSelection = NO;
        // Start where Keisho's own files are: "On My iPhone > Keisho".
        picker.directoryURL = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                                     inDomains:NSUserDomainMask]
                                  .firstObject;

        SM2FolderPickerDelegate* delegate = [[SM2FolderPickerDelegate alloc] init];
        delegate.key                      = @(key.c_str());
        delegate.done                     = new std::function<void(std::string)>(std::move(done));
        g_delegate                        = delegate;
        picker.delegate                   = delegate;
        [host presentViewController:picker animated:YES completion:nil];
    }
}

void restore_folder_access(Config* config)
{
    const std::array<std::pair<const char*, std::string*>, 7> fields = {{
        {"model1_rom_dir", &config->model1_rom_dir},
        {"model2_rom_dir", &config->model2_rom_dir},
        {"model3_rom_dir", &config->model3_rom_dir},
        {"nvram_dir", &config->nvram_dir},
        {"screenshot_dir", &config->screenshot_dir},
        {"bezel_dir", &config->bezel_dir},
        {"crosshair_dir", &config->crosshair_dir},
    }};
    @autoreleasepool {
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        for (const auto& [key, field] : fields) {
            NSString* name     = @(key);
            NSData*   bookmark = [defaults dataForKey:defaults_key(name)];
            if (bookmark == nil) continue;
            BOOL     stale = NO;
            NSError* error = nil;
            NSURL*   url   = [NSURL URLByResolvingBookmarkData:bookmark options:0 relativeToURL:nil
                                         bookmarkDataIsStale:&stale error:&error];
            if (url == nil) {
                SM2_WARN("folder picker: %s can no longer be opened (%s); pick it again in Settings > Paths",
                         key, error.localizedDescription.UTF8String);
                [defaults removeObjectForKey:defaults_key(name)];
                continue;
            }
            // On iOS a folder setting changes only through the picker, which
            // replaces the bookmark, so the bookmark is the setting.
            open_scope(url);
            if (stale) remember(url, name);
            *field = url.path.UTF8String;
        }
    }
}

}  // namespace sm2::osd
