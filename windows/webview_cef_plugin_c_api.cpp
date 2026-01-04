#include "include/webview_cef/webview_cef_plugin_c_api.h"

#include "webview_cef_plugin.h"

void WebviewCefPluginCApiRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar) {
    webview_cef::WebviewCefPlugin::RegisterWithRegistrar(registrar);
}

void initCEFProcesses(HINSTANCE hInstance) {
    webview_cef::initCEFProcesses(hInstance);
}

void handleWndProcForCEF(HWND hwnd, unsigned int message, WPARAM wParam, LPARAM lParam) {
    webview_cef::WebviewCefPlugin::handleMessageProc(hwnd, message, wParam, lParam);
}
