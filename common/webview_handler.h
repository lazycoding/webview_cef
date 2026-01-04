// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include "include/cef_client.h"

#include <functional>
#include <list>
#include <unordered_map>

#include "webview_cookieVisitor.h"

#define ColorUNDERLINE \
    0xFF000000  // Black SkColor value for underline,
                // same as Blink.
#define ColorBKCOLOR \
    0x00000000  // White SkColor value for background,
                // same as Blink.

struct browser_info {
    CefRefPtr<CefBrowser> browser;
    uint32_t width = 1;
    uint32_t height = 1;
    float dpi = 1.0;
    bool is_dragging = false;
    CefRect prev_ime_position = CefRect();
    bool is_ime_commit = false;
    double zoom_level = 0.0;
};
using FileDialogCallback =
        std::function<void(bool success, const std::vector<std::string>& file_path)>;
using OnFileDialogCallback = std::function<bool(
        CefRefPtr<CefBrowser> browser,
        CefDialogHandler::FileDialogMode mode,
        const CefString& title,
        const CefString& default_file_path,
        const std::vector<CefString>& accept_filters,
        FileDialogCallback callback)>;

class WebviewHandler : public CefClient,
                       public CefDisplayHandler,
                       public CefLifeSpanHandler,
                       public CefFocusHandler,
                       public CefRequestHandler,
                       public CefLoadHandler,
#ifdef OS_LINUX
                       public CefDialogHandler,
#endif
                       public CefRenderHandler {
    typedef cef_window_open_disposition_t WindowOpenDisposition;

public:
    // Forbid the following URLs from being loaded.
    std::list<std::string> forbiddenUrls;

    // Paint callback
    std::function<void(int browserId, const void* buffer, int32_t width, int32_t height)>
            onPaintCallback;
    // cef message event
    std::function<void(
            int browserId,
            bool isMainFrame,
            const std::string& url,
            bool userGesture,
            bool isRedirect)>
            onBeforeBrowseEvent;
    std::function<void(int browserId)> onAfterCreated;
    std::function<void(int browserId, bool isMainFrame, int transitionType)> onLoadStartEvent;
    std::function<void(int browserId, bool isMainFrame, int httpStatusCode)> onLoadEndEvent;
    std::function<void(int browserId, bool isMainFrame, int errorCode)> onLoadErrorEvent;
    std::function<void(int browserId, const std::string& url)> onUrlChangedEvent;
    std::function<void(int browserId, const std::string& title)> onTitleChangedEvent;
    std::function<void(int browserId, int type)> onCursorChangedEvent;
    std::function<void(int browserId, const std::string& text)> onTooltipEvent;
    std::function<void(
            int browserId,
            int level,
            const std::string& message,
            const std::string& source,
            int line)>
            onConsoleMessageEvent;
    std::function<void(int browserId, bool editable, const CefRect&)> onFocusedNodeChangeMessage;
    std::function<void(int browserId, int32_t x, int32_t y)> onImeCompositionRangeChangedMessage;
    // webpage message
    std::function<void(
            const std::string&,
            const std::string&,
            const std::string&,
            int browserId,
            const std::string&)>
            onJavaScriptChannelMessage;
    OnFileDialogCallback onOpenDialogHandler;

    explicit WebviewHandler();
    ~WebviewHandler();

    // CefClient methods:
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    virtual CefRefPtr<CefFocusHandler> GetFocusHandler() override {
        return this;
    }
    virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override {
        return this;
    }
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return this;
    }

#ifdef OS_LINUX
    virtual CefRefPtr<CefDialogHandler> GetDialogHandler() override {
        return this;
    }
#endif

    bool OnProcessMessageReceived(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefProcessId source_process,
            CefRefPtr<CefProcessMessage> message) override;

    // CefDisplayHandler methods:
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    virtual void OnAddressChange(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            const CefString& url) override;
    virtual bool OnCursorChange(
            CefRefPtr<CefBrowser> browser,
            CefCursorHandle cursor,
            cef_cursor_type_t type,
            const CefCursorInfo& custom_cursor_info) override;
    virtual bool OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) override;
    virtual bool OnConsoleMessage(
            CefRefPtr<CefBrowser> browser,
            cef_log_severity_t level,
            const CefString& message,
            const CefString& source,
            int line) override;

    // CefLifeSpanHandler methods:
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    virtual bool OnBeforePopup(
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
            bool* no_javascript_access) override;

    virtual void OnTakeFocus(CefRefPtr<CefBrowser> browser, bool next) override;
    virtual bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override;
    virtual void OnGotFocus(CefRefPtr<CefBrowser> browser) override;

    // CefRequestHandler methods
    virtual bool OnBeforeBrowse(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request,
            bool user_gesture,
            bool is_redirect) override;
    virtual bool OnCertificateError(
            CefRefPtr<CefBrowser> browser,
            cef_errorcode_t cert_error,
            const CefString& request_url,
            CefRefPtr<CefSSLInfo> ssl_info,
            CefRefPtr<CefCallback> callback) override;

    // CefLoadHandler methods:
    virtual void OnLoadStart(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            TransitionType transition_type) override;
    virtual void OnLoadEnd(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            int httpStatusCode) override;
    virtual void OnLoadError(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            ErrorCode errorCode,
            const CefString& errorText,
            const CefString& failedUrl) override;

    // CefRenderHandler methods:
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    virtual void OnPaint(
            CefRefPtr<CefBrowser> browser,
            PaintElementType type,
            const RectList& dirtyRects,
            const void* buffer,
            int width,
            int height) override;
    virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) override;
    virtual bool StartDragging(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefDragData> drag_data,
            DragOperationsMask allowed_ops,
            int x,
            int y) override;
    virtual void OnImeCompositionRangeChanged(
            CefRefPtr<CefBrowser> browser,
            const CefRange& selection_range,
            const CefRenderHandler::RectList& character_bounds) override;

#ifdef OS_LINUX
    virtual bool OnFileDialog(
            CefRefPtr<CefBrowser> browser,
            FileDialogMode mode,
            const CefString& title,
            const CefString& default_file_path,
            const std::vector<CefString>& accept_filters,
            CefRefPtr<CefFileDialogCallback> callback) override;
#endif
    // Request that all existing browser windows close.
    void CloseAllBrowsers(bool force_close);

    // Returns true if the Chrome runtime is enabled.
    static bool IsChromeRuntimeEnabled();

    void closeBrowser(int browserId);
    void createBrowser(const std::string& url, std::function<void(int)> callback);

    void saveZoomLevel(int browserId, double zoomLevel);
    void setZoomLevel(int browserId, double zoomLevel);
    void sendScrollEvent(int browserId, CefMouseEvent& ev, int deltaX, int deltaY);
    void changeSize(int browserId, float a_dpi, int width, int height);
    void cursorClick(int browserId, int buttonId, int x, int y, int clickCount, bool up);
    void cursorMove(int browserId, int buttonId, int x, int y, bool dragging);
    void sendKeyEvent(CefKeyEvent& ev);
    void loadUrl(int browserId, const std::string& url);
    void goForward(int browserId);
    void goBack(int browserId);
    void reload(int browserId);
    void openDevTools(int browserId);

    void imeSetComposition(int browserId, const std::string& text);
    void imeCommitText(int browserId, const std::string& text);
    void setClientFocus(int browserId, bool focus);

    void setCookie(const std::string& domain, const std::string& key, const std::string& value);
    void deleteCookie(const std::string& domain, const std::string& key);
    void visitAllCookies(
            std::function<void(std::map<std::string, std::map<std::string, std::string>>)>
                    callback);
    void visitUrlCookies(
            const std::string& domain,
            const bool& isHttpOnly,
            std::function<void(std::map<std::string, std::map<std::string, std::string>>)>
                    callback);

    void setJavaScriptChannels(int browserId, const std::vector<std::string> channels);
    void sendJavaScriptChannelCallBack(
            const bool error,
            const std::string result,
            const std::string callbackId,
            const bool deleteCallBackFunction,
            const int browserId,
            const std::string frameId);
    void executeJavaScript(
            int browserId,
            const std::string code,
            std::function<void(const std::string&)> callback = nullptr);

private:
    // List of existing browser windows. Only accessed on the CEF UI thread.
    std::unordered_map<int, browser_info> browser_map_;

    std::unordered_map<std::string, std::function<void(const std::string&)>> js_callbacks_;

    // Include the default reference counting implementation.
    IMPLEMENT_REFCOUNTING(WebviewHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
