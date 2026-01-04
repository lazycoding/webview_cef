// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "webview_handler.h"

#include <string>
#include <chrono>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include "webview_js_handler.h"

namespace {
// Thread-safe focused browser management
class FocusedBrowserManager {
public:
    static CefRefPtr<CefBrowser> Get() {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_focused_browser_;
    }

    static void Set(CefRefPtr<CefBrowser> browser) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_focused_browser_ = browser;
    }

private:
    static CefRefPtr<CefBrowser> current_focused_browser_;
    static std::mutex mutex_;
};

CefRefPtr<CefBrowser> FocusedBrowserManager::current_focused_browser_ = nullptr;
std::mutex FocusedBrowserManager::mutex_;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    static const std::string kDataURIPrefix = "data:";
    static const std::string kBase64Suffix = ";base64,";

    std::string encoded;
    encoded = CefBase64Encode(data.data(), data.size());
    return kDataURIPrefix + mime_type + kBase64Suffix + CefURIEncode(encoded, false).ToString();
}
}  // namespace

WebviewHandler::WebviewHandler() {}

WebviewHandler::~WebviewHandler() {
    browser_map_.clear();
    js_callbacks_.clear();
}

bool WebviewHandler::OnProcessMessageReceived(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) {
    std::string message_name = message->GetName();
    if (message_name == kFocusedNodeChangedMessage) {
        // current_focused_browser_ = browser;
        FocusedBrowserManager::Set(browser);
        bool editable = message->GetArgumentList()->GetBool(0);
        CefRect rect;
        rect.x = message->GetArgumentList()->GetInt(1);
        rect.y = message->GetArgumentList()->GetInt(2);
        rect.width = message->GetArgumentList()->GetInt(3);
        rect.height = message->GetArgumentList()->GetInt(4);
        onFocusedNodeChangeMessage(browser->GetIdentifier(), editable, rect);

        if (editable) {
            onImeCompositionRangeChangedMessage(
                    browser->GetIdentifier(), rect.x, rect.y + rect.height);
        }

    } else if (message_name == kJSCallCppFunctionMessage) {
        CefString fun_name = message->GetArgumentList()->GetString(0);
        CefString param = message->GetArgumentList()->GetString(1);
        int js_callback_id = message->GetArgumentList()->GetInt(2);

        if (fun_name.empty() || !(browser.get())) {
            return false;
        }

        onJavaScriptChannelMessage(
                fun_name,
                param,
                std::to_string(js_callback_id),
                browser->GetIdentifier(),
                std::to_string(frame->GetIdentifier()));
    } else if (message_name == kEvaluateCallbackMessage) {
        CefString callbackId = message->GetArgumentList()->GetString(0);
        CefString param = message->GetArgumentList()->GetString(1);
        if (!callbackId.empty() && !param.empty()) {
            auto it = js_callbacks_.find(callbackId.ToString());
            if (it != js_callbacks_.end()) {
                it->second(param.ToString());
                js_callbacks_.erase(it);
            }
        }
    }
    return false;
}

void WebviewHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    // todo: title change
    if (onTitleChangedEvent) {
        onTitleChangedEvent(browser->GetIdentifier(), title);
    }
}

void WebviewHandler::OnAddressChange(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& url) {
    if (onUrlChangedEvent) {
        onUrlChangedEvent(browser->GetIdentifier(), url);
    }
}

bool WebviewHandler::OnCursorChange(
        CefRefPtr<CefBrowser> browser,
        CefCursorHandle cursor,
        cef_cursor_type_t type,
        const CefCursorInfo& custom_cursor_info) {
    if (onCursorChangedEvent) {
        onCursorChangedEvent(browser->GetIdentifier(), type);
        return true;
    }
    return false;
}

bool WebviewHandler::OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) {
    if (onTooltipEvent) {
        onTooltipEvent(browser->GetIdentifier(), text);
        return true;
    }
    return false;
}

bool WebviewHandler::OnConsoleMessage(
        CefRefPtr<CefBrowser> browser,
        cef_log_severity_t level,
        const CefString& message,
        const CefString& source,
        int line) {
    if (onConsoleMessageEvent) {
        onConsoleMessageEvent(browser->GetIdentifier(), level, message, source, line);
    }
    return false;
}

void WebviewHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    if (!browser->IsPopup()) {
        browser_map_.emplace(browser->GetIdentifier(), browser_info());
        browser_map_[browser->GetIdentifier()].browser = browser;

        if (onAfterCreated) {
            onAfterCreated(browser->GetIdentifier());
        }
    }
}

bool WebviewHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    return false;
}

void WebviewHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    // 标记浏览器为关闭状态
    browser->GetHost()->WasHidden(true);

    auto it = browser_map_.find(browser->GetIdentifier());
    if (it != browser_map_.end()) {
        if (it->second.browser.get() && it->second.browser->IsSame(browser)) {
            DLOG(INFO) << "Closing browser id=" << browser->GetIdentifier();

            // 清除所有待处理的回调
            js_callbacks_.clear();
        }
    }

    if (FocusedBrowserManager::Get() && FocusedBrowserManager::Get()->IsSame(browser)) {
        FocusedBrowserManager::Set(nullptr);
    }
}

bool WebviewHandler::OnBeforePopup(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& target_url,
        const CefString& target_frame_name,
        WindowOpenDisposition target_disposition,
        bool user_gesture,
        const CefPopupFeatures& popupFeatures,
        CefWindowInfo& windowInfo,
        CefRefPtr<CefClient>& client,
        CefBrowserSettings& settings,
        CefRefPtr<CefDictionaryValue>& extra_info,
        bool* no_javascript_access) {
    loadUrl(browser->GetIdentifier(), target_url);
    return true;
}

void WebviewHandler::OnTakeFocus(CefRefPtr<CefBrowser> browser, bool next) {}

bool WebviewHandler::OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) {
    return false;
}

void WebviewHandler::OnGotFocus(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    FocusedBrowserManager::Set(browser);
}

bool WebviewHandler::OnBeforeBrowse(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool user_gesture,
        bool is_redirect) {
    // Forbid the following URLs from being loaded.
    bool result = false;
    for (auto& url : forbiddenUrls) {
        if (request->GetURL().ToString().find(url) != std::string::npos) {
            result = true;
            break;
        }
    }

    // Notify the client of the URL being loaded.
    if (onBeforeBrowseEvent) {
        onBeforeBrowseEvent(
                browser->GetIdentifier(),
                frame->IsMain(),
                request->GetURL().ToString(),
                user_gesture,
                is_redirect);
    }

    //
    return result;
}

bool WebviewHandler::OnCertificateError(
        CefRefPtr<CefBrowser> browser,
        cef_errorcode_t cert_error,
        const CefString& request_url,
        CefRefPtr<CefSSLInfo> ssl_info,
        CefRefPtr<CefCallback> callback) {
    return false;  // 默认处理
}

void WebviewHandler::OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transition_type) {
    if (onLoadStartEvent) {
        onLoadStartEvent(browser->GetIdentifier(), frame->IsMain(), transition_type);
    }
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it != browser_map_.end()) {
        browser->GetHost()->SetZoomLevel(it->second.zoom_level);
    }
}

void WebviewHandler::OnLoadEnd(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int httpStatusCode) {
    if (onLoadEndEvent) {
        onLoadEndEvent(browser->GetIdentifier(), frame->IsMain(), httpStatusCode);
    }
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it != browser_map_.end()) {
        browser->GetHost()->SetZoomLevel(it->second.zoom_level);
    }
}

void WebviewHandler::OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString& errorText,
        const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();

    // Allow Chrome to show the error page.
    if (IsChromeRuntimeEnabled()) {
        return;
    }

    // Don't display an error for downloaded files.
    if (errorCode == ERR_ABORTED) {
        return;
    }

    // Only show error page for main frame
    if (!frame->IsMain()) {
        return;
    }

    // Format error page
    static const std::string kErrorPageTemplate =
            "<html><head>"
            "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
            "<style>body{font-family:sans-serif;margin:2em;color:#333;background:#fff;}"
            "h2{color:#d32f2f;}</style></head>"
            "<body><h2>Failed to load URL: %s</h2>"
            "<p>Error: %s (%d)</p></body></html>";

    char error_page[1024];
    snprintf(
            error_page,
            sizeof(error_page),
            kErrorPageTemplate.c_str(),
            failedUrl.ToString().c_str(),
            errorText.ToString().c_str(),
            static_cast<int>(errorCode));

    frame->LoadURL(GetDataURI(error_page, "text/html"));

    if (onLoadErrorEvent) {
        onLoadErrorEvent(browser->GetIdentifier(), frame->IsMain(), errorCode);
    }
}

void WebviewHandler::CloseAllBrowsers(bool force_close) {
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::BindOnce(&WebviewHandler::CloseAllBrowsers, this, force_close));
        return;
    }
    if (browser_map_.empty()) {
        DLOG(WARNING) << "Attempted to close all browsers when none exist";
        return;
    }

    DLOG(INFO) << "Closing " << browser_map_.size() << " browsers";

    for (auto& it : browser_map_) {
        if (it.second.browser.get()) {
            it.second.browser->GetHost()->CloseBrowser(force_close);
            it.second.browser = nullptr;
        }
    }
    browser_map_.clear();
    FocusedBrowserManager::Set(nullptr);
}

// static
bool WebviewHandler::IsChromeRuntimeEnabled() {
    static int value = -1;
    if (value == -1) {
        CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
        value = command_line->HasSwitch("enable-chrome-runtime") ? 1 : 0;
    }
    return value == 1;
}

void WebviewHandler::closeBrowser(int browserId) {
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::BindOnce(&WebviewHandler::closeBrowser, this, browserId));
        return;
    }

    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        DLOG(INFO) << "Closing browser id=" << browserId;
        if (it->second.browser.get()) {
            it->second.browser->GetHost()->CloseBrowser(true);
            it->second.browser = nullptr;
        }
        browser_map_.erase(it);

        if (FocusedBrowserManager::Get() &&
            FocusedBrowserManager::Get()->GetIdentifier() == browserId) {
            FocusedBrowserManager::Set(nullptr);
        }
    } else {
        DLOG(WARNING) << "Attempted to close non-existent browser " << browserId;
    }
}

void WebviewHandler::createBrowser(const std::string& url, std::function<void(int)> callback) {
#ifndef OS_MAC
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::BindOnce(&WebviewHandler::createBrowser, this, url, callback));
        return;
    }
#endif
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 30;
    browser_settings.local_storage = STATE_ENABLED;
    browser_settings.databases = STATE_ENABLED;
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);
    callback(
            CefBrowserHost::CreateBrowserSync(
                    window_info, this, url, browser_settings, nullptr, nullptr)
                    ->GetIdentifier());
}

void WebviewHandler::saveZoomLevel(int browserId, double zoomLevel) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.zoom_level = zoomLevel;
    }
}

void WebviewHandler::setZoomLevel(int browserId, double zoomLevel) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.zoom_level = zoomLevel;
        it->second.browser->GetHost()->SetZoomLevel(zoomLevel);
    }
}

void WebviewHandler::sendScrollEvent(int browserId, CefMouseEvent& ev, int deltaX, int deltaY) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
#ifndef __APPLE__
        // The scrolling direction on Windows and Linux is different from MacOS
        deltaY = -deltaY;
        // Flutter scrolls too slowly, it looks more normal by 10x default speed.
        it->second.browser->GetHost()->SendMouseWheelEvent(ev, deltaX * 10, deltaY * 10);
#else
        it->second.browser->GetHost()->SendMouseWheelEvent(ev, deltaX, deltaY);
#endif
    }
}

void WebviewHandler::changeSize(int browserId, float a_dpi, int w, int h) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.dpi = a_dpi;
        it->second.width = w;
        it->second.height = h;
        it->second.browser->GetHost()->WasResized();
    }
}

void WebviewHandler::cursorClick(
        int browserId,
        int buttonId,
        int x,
        int y,
        int clickCount,
        bool up) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        CefMouseEvent ev;
        ev.x = x;
        ev.y = y;

        if (up) {
            ev.modifiers = EVENTFLAG_NONE;
        } else {
            // buttonId: 1: left, 2: right, 4: middle
            switch (buttonId) {
                case 1:
                    ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
                    break;
                case 2:
                    ev.modifiers = EVENTFLAG_RIGHT_MOUSE_BUTTON;
                    break;
                case 4:
                    ev.modifiers = EVENTFLAG_MIDDLE_MOUSE_BUTTON;
                    break;
                default:
                    ev.modifiers = EVENTFLAG_NONE;
            }
        }
        if (up && it->second.is_dragging) {
            it->second.browser->GetHost()->DragTargetDrop(ev);
            it->second.browser->GetHost()->DragSourceSystemDragEnded();
            it->second.is_dragging = false;
        } else {
            auto flag = CefBrowserHost::MouseButtonType::MBT_LEFT;
            switch (buttonId) {
                case 1:
                    flag = CefBrowserHost::MouseButtonType::MBT_LEFT;
                    break;
                case 2:
                    flag = CefBrowserHost::MouseButtonType::MBT_RIGHT;
                    break;
                case 4:
                    flag = CefBrowserHost::MouseButtonType::MBT_MIDDLE;
                    break;
            }
            if (clickCount < 1) {
                clickCount = 1;
            }
            it->second.browser->GetHost()->SendMouseClickEvent(ev, flag, up, clickCount);
        }
    }
}

void WebviewHandler::cursorMove(int browserId, int /*buttonId*/, int x, int y, bool dragging) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        CefMouseEvent ev;
        ev.x = x;
        ev.y = y;
        if (dragging) {
            ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
        }
        if (it->second.is_dragging && dragging) {
            it->second.browser->GetHost()->DragTargetDragOver(ev, DRAG_OPERATION_EVERY);
        } else {
            it->second.browser->GetHost()->SendMouseMoveEvent(ev, false);
        }
    }
}

bool WebviewHandler::StartDragging(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDragData> drag_data,
        DragOperationsMask allowed_ops,
        int x,
        int y) {
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it != browser_map_.end() && it->second.browser->IsSame(browser)) {
        CefMouseEvent ev;
        ev.x = x;
        ev.y = y;
        ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
        it->second.browser->GetHost()->DragTargetDragEnter(drag_data, ev, DRAG_OPERATION_EVERY);
        it->second.is_dragging = true;
    }
    return true;
}

// std::string _printRect(const CefRenderHandler::RectList& character_bounds) {
//     int i = 0;
//     std::stringstream stream;
//     for (auto& bound : character_bounds) {
//         stream << "Rect[" << i << "]=(" << bound.x << "," << bound.y << "," << bound.width << ","
//                << bound.height << ")" << std::endl;
//     }
//     return stream.str();
// }

void WebviewHandler::OnImeCompositionRangeChanged(
        CefRefPtr<CefBrowser> browser,
        const CefRange& selection_range,
        const CefRenderHandler::RectList& character_bounds) {
    CEF_REQUIRE_UI_THREAD();
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it == browser_map_.end() || !it->second.browser.get() || browser->IsPopup()) {
        return;
    }
    if (!character_bounds.empty()) {
        if (it->second.is_ime_commit) {
            auto lastCharacter = character_bounds.back();
            it->second.prev_ime_position = lastCharacter;
            onImeCompositionRangeChangedMessage(
                    browser->GetIdentifier(),
                    lastCharacter.x + lastCharacter.width,
                    lastCharacter.y + lastCharacter.height);
            it->second.is_ime_commit = false;
        } else {
            auto firstCharacter = character_bounds.front();
            if (firstCharacter != it->second.prev_ime_position) {
                it->second.prev_ime_position = firstCharacter;
                onImeCompositionRangeChangedMessage(
                        browser->GetIdentifier(),
                        firstCharacter.x,
                        firstCharacter.y + firstCharacter.height);
            }
        }
    }
}

#if defined(OS_LINUX)

static void ExecuteCallback(
        CefRefPtr<CefFileDialogCallback> callback,
        bool success,
        const std::vector<std::string>& file_paths) {
    if (success) {
        std::vector<CefString> file_paths_cef;
        file_paths_cef.reserve(file_paths.size());
        for (const auto& path : file_paths) {
            file_paths_cef.push_back(path);
        }
        callback->Continue(file_paths_cef);
    } else {
        callback->Cancel();
    }
}

bool WebviewHandler::OnFileDialog(
        CefRefPtr<CefBrowser> browser,
        FileDialogMode mode,
        const CefString& title,
        const CefString& default_file_path,
        const std::vector<CefString>& accept_filters,
        CefRefPtr<CefFileDialogCallback> callback) {
    CEF_REQUIRE_UI_THREAD();
    if (onOpenDialogHandler) {
        return onOpenDialogHandler(
                browser,
                mode,
                title,
                default_file_path,
                accept_filters,
                [callback](bool success, const std::vector<std::string>& paths) {
                    if (!CefCurrentlyOn(TID_UI)) {
                        CefPostTask(
                                TID_UI, base::BindOnce(ExecuteCallback, callback, success, paths));
                    } else {
                        ExecuteCallback(callback, success, paths);
                    }
                });
    }
    return false;
}
#endif

void WebviewHandler::sendKeyEvent(CefKeyEvent& ev) {
    if (auto browser = FocusedBrowserManager::Get()) {
        if (!CefCurrentlyOn(TID_UI)) {
            DLOG(ERROR) << "sendKeyEvent called from non-UI thread";
            return;
        }
        browser->GetHost()->SendKeyEvent(ev);
    } else {
        DLOG(INFO) << "Attempted to send key event with no focused browser";
    }
}

void WebviewHandler::loadUrl(int browserId, const std::string& url) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.browser->GetMainFrame()->LoadURL(url);
    }
}

void WebviewHandler::goForward(int browserId) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.browser->GetMainFrame()->GetBrowser()->GoForward();
    }
}

void WebviewHandler::goBack(int browserId) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.browser->GetMainFrame()->GetBrowser()->GoBack();
    }
}

void WebviewHandler::reload(int browserId) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        it->second.browser->GetMainFrame()->GetBrowser()->Reload();
    }
}

void WebviewHandler::openDevTools(int browserId) {
    auto it = browser_map_.find(browserId);
    if (it != browser_map_.end()) {
        CefWindowInfo windowInfo;
#ifdef OS_WIN
        windowInfo.SetAsPopup(nullptr, "DevTools");
#endif
        it->second.browser->GetHost()->ShowDevTools(
                windowInfo, this, CefBrowserSettings(), CefPoint());
    }
}

void WebviewHandler::imeSetComposition(int browserId, const std::string& text) {
    auto it = browser_map_.find(browserId);
    if (it == browser_map_.end() || !it->second.browser.get()) {
        return;
    }

    CefString cTextStr = text;

    std::vector<CefCompositionUnderline> underlines;
    cef_composition_underline_t underline = {};
    underline.range.from = 0;
    underline.range.to = static_cast<int>(0 + cTextStr.length());
    underline.color = ColorUNDERLINE;
    underline.background_color = ColorBKCOLOR;
    underline.thick = 0;
    underline.style = CEF_CUS_DOT;
    underlines.push_back(underline);

    // Keeps the caret at the end of the composition
    auto selection_range_end = static_cast<int>(0 + cTextStr.length());
    CefRange selection_range = CefRange(0, selection_range_end);
    it->second.browser->GetHost()->ImeSetComposition(
            cTextStr, underlines, CefRange(UINT32_MAX, UINT32_MAX), selection_range);
}

void WebviewHandler::imeCommitText(int browserId, const std::string& text) {
    auto it = browser_map_.find(browserId);
    if (it == browser_map_.end() || !it->second.browser.get()) {
        return;
    }

    CefString cTextStr = CefString(text);
    it->second.is_ime_commit = true;

    std::vector<CefCompositionUnderline> underlines;
    auto selection_range_end = static_cast<int>(0 + cTextStr.length());
    CefRange selection_range = CefRange(selection_range_end, selection_range_end);
    // Linux 上在 ImeCommitText 之前执行 ImeSetComposition 代码会导致 slate
    // 编辑器中文输入不生效，所以先注释掉，具体原理待学习
    // #ifndef _WIN32
    //     it->second.browser->GetHost()->ImeSetComposition(
    //             cTextStr, underlines, CefRange(UINT32_MAX, UINT32_MAX), selection_range);
    // #endif
    it->second.browser->GetHost()->ImeCommitText(cTextStr, CefRange(UINT32_MAX, UINT32_MAX), 0);
}

void WebviewHandler::setClientFocus(int browserId, bool focus) {
    auto it = browser_map_.find(browserId);
    if (it == browser_map_.end() || !it->second.browser.get()) {
        return;
    }
    it->second.browser->GetHost()->SetFocus(focus);
}

void WebviewHandler::setCookie(
        const std::string& domain,
        const std::string& key,
        const std::string& value) {
    CefRefPtr<CefCookieManager> manager = CefCookieManager::GetGlobalManager(nullptr);
    if (manager) {
        CefCookie cookie;
        CefString(&cookie.path).FromASCII("/");
        CefString(&cookie.name).FromString(key.c_str());
        CefString(&cookie.value).FromString(value.c_str());

        if (!domain.empty()) {
            CefString(&cookie.domain).FromString(domain.c_str());
        }

        cookie.httponly = true;
        cookie.secure = false;
        std::string httpDomain = "https://" + domain + "/cookiestorage";
        manager->SetCookie(httpDomain, cookie, nullptr);
    }
}

void WebviewHandler::deleteCookie(const std::string& domain, const std::string& key) {
    CefRefPtr<CefCookieManager> manager = CefCookieManager::GetGlobalManager(nullptr);
    if (manager) {
        std::string httpDomain = "https://" + domain + "/cookiestorage";
        manager->DeleteCookies(httpDomain, key, nullptr);
    }
}

void WebviewHandler::visitAllCookies(
        std::function<void(std::map<std::string, std::map<std::string, std::string>>)> callback) {
    CefRefPtr<CefCookieManager> manager = CefCookieManager::GetGlobalManager(nullptr);
    if (!manager) {
        return;
    }

    CefRefPtr<WebviewCookieVisitor> cookieVisitor = new WebviewCookieVisitor();
    cookieVisitor->setOnVisitComplete(callback);

    manager->VisitAllCookies(cookieVisitor);
}

void WebviewHandler::visitUrlCookies(
        const std::string& domain,
        const bool& isHttpOnly,
        std::function<void(std::map<std::string, std::map<std::string, std::string>>)> callback) {
    CefRefPtr<CefCookieManager> manager = CefCookieManager::GetGlobalManager(nullptr);
    if (!manager) {
        return;
    }

    CefRefPtr<WebviewCookieVisitor> cookieVisitor = new WebviewCookieVisitor();
    cookieVisitor->setOnVisitComplete(callback);

    std::string httpDomain = "https://" + domain + "/cookiestorage";

    manager->VisitUrlCookies(httpDomain, isHttpOnly, cookieVisitor);
}

void WebviewHandler::setJavaScriptChannels(int browserId, const std::vector<std::string> channels) {
    std::string extensionCode = "";
    for (auto& channel : channels) {
        extensionCode += channel;
        extensionCode += " = (e,r) => {external.JavaScriptChannel('";
        extensionCode += channel;
        extensionCode += "',e,r)};";
    }
    executeJavaScript(browserId, extensionCode);
}

void WebviewHandler::sendJavaScriptChannelCallBack(
        const bool error,
        const std::string result,
        const std::string callbackId,
        const bool deleteCallBackFunction,
        const int browserId,
        const std::string frameId) {
    CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(kExecuteJsCallbackMessage);
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    args->SetInt(0, atoi(callbackId.c_str()));
    args->SetBool(1, error);
    args->SetString(2, result);
    args->SetBool(3, deleteCallBackFunction);
    auto bit = browser_map_.find(browserId);
    if (bit != browser_map_.end()) {
        CefRefPtr<CefFrame> frame = bit->second.browser->GetMainFrame();
        if (frame->GetIdentifier() == atoll(frameId.c_str())) {
            frame->SendProcessMessage(PID_RENDERER, message);
        }
    }
}

static std::string GetCallbackId() {
    auto time = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now());
    time_t timestamp = time.time_since_epoch().count();
    return std::to_string(timestamp);
}

void WebviewHandler::executeJavaScript(
        int browserId,
        const std::string code,
        std::function<void(const std::string&)> callback) {
    if (!code.empty()) {
        auto bit = browser_map_.find(browserId);
        if (bit != browser_map_.end() && bit->second.browser.get()) {
            // TODO: this code is not true on muti tab
            CefRefPtr<CefFrame> frame = bit->second.browser->GetMainFrame();
            if (frame) {
                std::string finalCode = code;
                if (callback != nullptr) {
                    std::string callbackId = GetCallbackId();
                    finalCode = "external.EvaluateCallback('";
                    finalCode += callbackId;
                    finalCode += "',(function(){return ";
                    finalCode += code;
                    finalCode += "})());";
                    js_callbacks_[callbackId] = callback;
                }
                frame->ExecuteJavaScript(finalCode, frame->GetURL(), 0);
            }
        }
    }
}

void WebviewHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    CEF_REQUIRE_UI_THREAD();

    // 确保浏览器对象有效且未被关闭
    if (!browser.get()) {
        rect.x = rect.y = 0;
        rect.width = rect.height = 1;
        return;
    }

    auto it = browser_map_.find(browser->GetIdentifier());
    if (it == browser_map_.end() || !it->second.browser.get() || browser->IsPopup()) {
        // 当浏览器正在关闭时返回最小有效尺寸避免崩溃
        rect.x = rect.y = 0;
        rect.width = rect.height = 1;
        DLOG_IF(INFO, it == browser_map_.end())
                << "GetViewRect called for unknown browser " << browser->GetIdentifier();
        return;
    }

    rect.x = rect.y = 0;

    // 确保返回有效的最小尺寸
    static uint32_t min_size = 1;
    rect.width = std::max(min_size, it->second.width);
    rect.height = std::max(min_size, it->second.height);
}

bool WebviewHandler::GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) {
    if (browser_map_.find(browser->GetIdentifier()) != browser_map_.end()) {
        screen_info.device_scale_factor = browser_map_[browser->GetIdentifier()].dpi;
        return true;
    }
    return false;
}

void WebviewHandler::OnPaint(
        CefRefPtr<CefBrowser> browser,
        CefRenderHandler::PaintElementType type,
        const CefRenderHandler::RectList& dirtyRects,
        const void* buffer,
        int w,
        int h) {
    if (!browser->IsPopup() && onPaintCallback != nullptr) {
        onPaintCallback(browser->GetIdentifier(), buffer, w, h);
    }
}
