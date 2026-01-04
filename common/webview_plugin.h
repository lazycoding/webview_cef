#ifndef WEBVIEW_PLUGIN_H
#define WEBVIEW_PLUGIN_H

#include "webview_value.h"
#include "webview_app.h"
#include <include/cef_base.h>

#include <functional>
namespace webview_cef {
class WebviewTexture {
public:
    virtual ~WebviewTexture() {}
    virtual void onFrame(const void* buffer, int width, int height) {}
    int64_t textureId = 0;
    bool isFocused = false;
};
class WebviewPlugin {
public:
    WebviewPlugin();
    ~WebviewPlugin();
    void initCallback();
    void uninitCallback();
    void HandleMethodCall(
            const std::string& name,
            WValue* values,
            std::function<void(int, WValue*)> result);
    void sendKeyEvent(CefKeyEvent& ev);
    void setInvokeMethodFunc(std::function<void(const std::string&, WValue*)> func);
    void setCreateTextureFunc(std::function<std::shared_ptr<WebviewTexture>()> func);
    bool getAnyBrowserFocused();
    void setHandleDialogFunc(OnFileDialogCallback func);

private:
    int cursorAction(const std::string& name, WValue* args);
    std::function<void(const std::string&, WValue*)> m_invokeFlutterMethod;
    std::function<std::shared_ptr<WebviewTexture>()> m_createTextureFunc;
    CefRefPtr<WebviewHandler> m_webviewHandler;
    std::unordered_map<int, std::shared_ptr<WebviewTexture>> m_renderers;
    bool m_init = false;
    std::string m_jsCode;
};

#ifdef _WIN32
void initCEFProcesses(HINSTANCE hInstance);
#else
void initCEFProcesses(int argc, char* argv[]);
#endif

void startCEF();

void stopCEF();

void doMessageLoopWork();

void SwapBufferFromBgraToRgba(void* _dest, const void* _src, int width, int height);
}  // namespace webview_cef

#endif  // WEBVIEW_PLUGIN_H
