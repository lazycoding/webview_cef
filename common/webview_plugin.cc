#include "webview_plugin.h"

#ifdef OS_MAC
#include <include/wrapper/cef_library_loader.h>
#endif

#include <math.h>
#include <memory>
#include <thread>
#include <iostream>
#include <unordered_map>

namespace webview_cef {
CefMainArgs mainArgs;
CefRefPtr<WebviewApp> gWebviewApp;
CefString userAgent;
CefString localCachePath;
bool isCefInitialized = false;

WebviewPlugin::WebviewPlugin() {
    m_webviewHandler = new WebviewHandler();
}

WebviewPlugin::~WebviewPlugin() {
    uninitCallback();
    m_webviewHandler->CloseAllBrowsers(true);
    m_webviewHandler = nullptr;
    if (!m_renderers.empty()) {
        m_renderers.clear();
    }
}

void WebviewPlugin::initCallback() {
    if (!m_init) {
        m_webviewHandler->onPaintCallback =
                [=](int browserId, const void* buffer, int32_t width, int32_t height) {
                    if (m_renderers.find(browserId) != m_renderers.end() &&
                        m_renderers[browserId] != nullptr) {
                        m_renderers[browserId]->onFrame(buffer, width, height);
                    }
                };

        m_webviewHandler->onTooltipEvent = [=](int browserId, const std::string& text) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* wText = webview_value_new_string(const_cast<char*>(text.c_str()));
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "text", wText);
                m_invokeFlutterMethod("onTooltip", retMap);
                webview_value_unref(bId);
                webview_value_unref(wText);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onCursorChangedEvent = [=](int browserId, int type) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* wType = webview_value_new_int(type);
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "type", wType);
                m_invokeFlutterMethod("onCursorChanged", retMap);
                webview_value_unref(bId);
                webview_value_unref(wType);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onConsoleMessageEvent = [=](int browserId,
                                                      int level,
                                                      const std::string& message,
                                                      const std::string& source,
                                                      int line) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* wLevel = webview_value_new_int(level);
                WValue* wMessage = webview_value_new_string(const_cast<char*>(message.c_str()));
                WValue* wSource = webview_value_new_string(const_cast<char*>(source.c_str()));
                WValue* wLine = webview_value_new_int(line);
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "level", wLevel);
                webview_value_set_string(retMap, "message", wMessage);
                webview_value_set_string(retMap, "source", wSource);
                webview_value_set_string(retMap, "line", wLine);
                m_invokeFlutterMethod("onConsoleMessage", retMap);
                webview_value_unref(bId);
                webview_value_unref(wLevel);
                webview_value_unref(wMessage);
                webview_value_unref(wSource);
                webview_value_unref(wLine);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onBeforeBrowseEvent = [=](int browserId,
                                                    bool isMainFrame,
                                                    const std::string& url,
                                                    bool userGesture,
                                                    bool isRedirect) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* isMF = webview_value_new_bool(isMainFrame);
                WValue* wUrl = webview_value_new_string(const_cast<char*>(url.c_str()));
                WValue* uGes = webview_value_new_bool(userGesture);
                WValue* isRed = webview_value_new_bool(isRedirect);
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "isMainFrame", isMF);
                webview_value_set_string(retMap, "url", wUrl);
                webview_value_set_string(retMap, "userGesture", uGes);
                webview_value_set_string(retMap, "isRedirect", isRed);
                m_invokeFlutterMethod("onBeforeBrowse", retMap);
                webview_value_unref(bId);
                webview_value_unref(isMF);
                webview_value_unref(wUrl);
                webview_value_unref(uGes);
                webview_value_unref(isRed);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onLoadStartEvent =
                [=](int browserId, bool isMainFrame, int transitionType) {
                    if (!isMainFrame)
                        return;
                    if (!m_jsCode.empty()) {
                        m_webviewHandler->executeJavaScript(browserId, m_jsCode, nullptr);
                    }
                    if (m_invokeFlutterMethod) {
                        WValue* bId = webview_value_new_int(browserId);
                        WValue* isMF = webview_value_new_bool(isMainFrame);
                        WValue* transType = webview_value_new_int(transitionType);
                        WValue* retMap = webview_value_new_map();
                        webview_value_set_string(retMap, "browserId", bId);
                        webview_value_set_string(retMap, "isMainFrame", isMF);
                        webview_value_set_string(retMap, "transitionType", transType);
                        m_invokeFlutterMethod("onLoadStart", retMap);
                        webview_value_unref(bId);
                        webview_value_unref(isMF);
                        webview_value_unref(transType);
                        webview_value_unref(retMap);
                    }
                };

        m_webviewHandler->onLoadEndEvent =
                [=](int browserId, bool isMainFrame, int httpStatusCode) {
                    if (m_invokeFlutterMethod) {
                        WValue* bId = webview_value_new_int(browserId);
                        WValue* isMF = webview_value_new_bool(isMainFrame);
                        WValue* httpCode = webview_value_new_int(httpStatusCode);
                        WValue* retMap = webview_value_new_map();
                        webview_value_set_string(retMap, "browserId", bId);
                        webview_value_set_string(retMap, "isMainFrame", isMF);
                        webview_value_set_string(retMap, "httpStatusCode", httpCode);
                        m_invokeFlutterMethod("onLoadEnd", retMap);
                        webview_value_unref(bId);
                        webview_value_unref(isMF);
                        webview_value_unref(httpCode);
                        webview_value_unref(retMap);
                    }
                    if (!m_jsCode.empty()) {
                        m_webviewHandler->executeJavaScript(browserId, m_jsCode, nullptr);
                    }
                };

        m_webviewHandler->onLoadErrorEvent = [=](int browserId, bool isMainFrame, int errorCode) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* isMF = webview_value_new_bool(isMainFrame);
                WValue* errCode = webview_value_new_int(errorCode);
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "isMainFrame", isMF);
                webview_value_set_string(retMap, "errorCode", errCode);
                m_invokeFlutterMethod("onLoadError", retMap);
                webview_value_unref(bId);
                webview_value_unref(isMF);
                webview_value_unref(errCode);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onUrlChangedEvent = [=](int browserId, const std::string& url) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* wUrl = webview_value_new_string(const_cast<char*>(url.c_str()));
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "url", wUrl);
                m_invokeFlutterMethod("onUrlChanged", retMap);
                webview_value_unref(bId);
                webview_value_unref(wUrl);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onTitleChangedEvent = [=](int browserId, const std::string& title) {
            if (m_invokeFlutterMethod) {
                WValue* bId = webview_value_new_int(browserId);
                WValue* wTitle = webview_value_new_string(const_cast<char*>(title.c_str()));
                WValue* retMap = webview_value_new_map();
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "title", wTitle);
                m_invokeFlutterMethod("onTitleChanged", retMap);
                webview_value_unref(bId);
                webview_value_unref(wTitle);
                webview_value_unref(retMap);
            }
        };

        m_webviewHandler->onJavaScriptChannelMessage = [=](const std::string& channelName,
                                                           const std::string& message,
                                                           const std::string& callbackId,
                                                           int browserId,
                                                           const std::string& frameId) {
            if (m_invokeFlutterMethod) {
                WValue* retMap = webview_value_new_map();
                WValue* channel = webview_value_new_string(const_cast<char*>(channelName.c_str()));
                WValue* msg = webview_value_new_string(const_cast<char*>(message.c_str()));
                WValue* cbId = webview_value_new_string(const_cast<char*>(callbackId.c_str()));
                WValue* bId = webview_value_new_int(browserId);
                WValue* fId = webview_value_new_string(const_cast<char*>(frameId.c_str()));
                webview_value_set_string(retMap, "channel", channel);
                webview_value_set_string(retMap, "message", msg);
                webview_value_set_string(retMap, "callbackId", cbId);
                webview_value_set_string(retMap, "browserId", bId);
                webview_value_set_string(retMap, "frameId", fId);
                m_invokeFlutterMethod("javascriptChannelMessage", retMap);
                webview_value_unref(retMap);
                webview_value_unref(channel);
                webview_value_unref(msg);
                webview_value_unref(cbId);
                webview_value_unref(bId);
                webview_value_unref(fId);
            }
        };

        m_webviewHandler->onFocusedNodeChangeMessage =
                [=](int nBrowserId, bool bEditable, const CefRect& rect) {
                    if (m_invokeFlutterMethod) {
                        WValue* bId = webview_value_new_int(int64_t(nBrowserId));
                        WValue* editable = webview_value_new_bool(bEditable);
                        WValue* retMap = webview_value_new_map();
                        webview_value_set_string(retMap, "browserId", bId);
                        webview_value_set_string(retMap, "editable", editable);
                        m_invokeFlutterMethod("onFocusedNodeChangeMessage", retMap);
                        webview_value_unref(bId);
                        webview_value_unref(editable);
                        webview_value_unref(retMap);
                    }
                };

        m_webviewHandler->onImeCompositionRangeChangedMessage =
                [=](int nBrowserId, int32_t x, int32_t y) {
                    if (m_invokeFlutterMethod) {
                        WValue* bId = webview_value_new_int(nBrowserId);
                        WValue* retMap = webview_value_new_map();
                        WValue* xValue = webview_value_new_int(x);
                        WValue* yValue = webview_value_new_int(y);
                        webview_value_set_string(retMap, "browserId", bId);
                        webview_value_set_string(retMap, "x", xValue);
                        webview_value_set_string(retMap, "y", yValue);
                        m_invokeFlutterMethod("onImeCompositionRangeChangedMessage", retMap);
                        webview_value_unref(bId);
                        webview_value_unref(xValue);
                        webview_value_unref(yValue);
                        webview_value_unref(retMap);
                    }
                };
        m_init = true;
    }
}

void WebviewPlugin::uninitCallback() {
    m_webviewHandler->onPaintCallback = nullptr;
    m_webviewHandler->onTooltipEvent = nullptr;
    m_webviewHandler->onCursorChangedEvent = nullptr;
    m_webviewHandler->onConsoleMessageEvent = nullptr;
    m_webviewHandler->onBeforeBrowseEvent = nullptr;
    m_webviewHandler->onLoadStartEvent = nullptr;
    m_webviewHandler->onLoadEndEvent = nullptr;
    m_webviewHandler->onLoadErrorEvent = nullptr;
    m_webviewHandler->onUrlChangedEvent = nullptr;
    m_webviewHandler->onTitleChangedEvent = nullptr;
    m_webviewHandler->onJavaScriptChannelMessage = nullptr;
    m_webviewHandler->onFocusedNodeChangeMessage = nullptr;
    m_webviewHandler->onImeCompositionRangeChangedMessage = nullptr;
    m_webviewHandler->onOpenDialogHandler = nullptr;
    m_init = false;
}

void WebviewPlugin::HandleMethodCall(
        const std::string& name,
        WValue* values,
        std::function<void(int, WValue*)> result) {
    if (name.compare("init") == 0) {
        if (!isCefInitialized) {
            if (values != nullptr) {
                userAgent = CefString(
                        webview_value_get_string(webview_value_get_list_value(values, 0)));
                localCachePath = CefString(
                        webview_value_get_string(webview_value_get_list_value(values, 1)));
            }
            startCEF();
        }
        initCallback();
        result(1, nullptr);
    } else if (name.compare("injectJs") == 0) {
        std::string code = webview_value_get_string(values);
        m_jsCode.swap(code);
        result(1, nullptr);
    } else if (name.compare("quit") == 0) {
        // only call this method when you want to quit the gWebviewApp
        stopCEF();
        result(1, nullptr);
    } else if (name.compare("create") == 0) {
        const auto url = webview_value_get_string(webview_value_get_list_value(values, 0));
        double zoom = webview_value_get_double(webview_value_get_list_value(values, 1));
        // values 2 is forbidden urls list string, split by ','
        std::string forbiddenUrl =
                webview_value_get_string(webview_value_get_list_value(values, 2));
        m_webviewHandler->forbiddenUrls = std::list<std::string>();
        if (!forbiddenUrl.empty()) {
            std::string::size_type pos = 0;
            std::string::size_type prePos = 0;
            while ((pos = forbiddenUrl.find(',', prePos)) != std::string::npos) {
                m_webviewHandler->forbiddenUrls.push_back(
                        forbiddenUrl.substr(prePos, pos - prePos));
                prePos = pos + 1;
            }
            m_webviewHandler->forbiddenUrls.push_back(forbiddenUrl.substr(prePos));
        }
        m_webviewHandler->onAfterCreated = [this, zoom](int browserId) {
            m_webviewHandler->saveZoomLevel(browserId, zoom);
        };
        m_webviewHandler->createBrowser(url, [=](int browserId) {
            std::shared_ptr<WebviewTexture> renderer = m_createTextureFunc();
            m_renderers[browserId] = renderer;
            WValue* response = webview_value_new_list();
            webview_value_append(response, webview_value_new_int(browserId));
            webview_value_append(response, webview_value_new_int(renderer->textureId));
            result(1, response);
            webview_value_unref(response);
        });
    } else if (name.compare("close") == 0) {
        int browserId = int(webview_value_get_int(values));
        m_webviewHandler->closeBrowser(browserId);
        if (m_renderers.find(browserId) != m_renderers.end() && m_renderers[browserId] != nullptr) {
            m_renderers[browserId].reset();
        }
        result(1, nullptr);
    } else if (name.compare("loadUrl") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto url = webview_value_get_string(webview_value_get_list_value(values, 1));
        if (url != nullptr) {
            m_webviewHandler->loadUrl(browserId, url);
            result(1, nullptr);
        }
    } else if (name.compare("setSize") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto dpi = webview_value_get_double(webview_value_get_list_value(values, 1));
        const auto width = webview_value_get_double(webview_value_get_list_value(values, 2));
        const auto height = webview_value_get_double(webview_value_get_list_value(values, 3));
        m_webviewHandler->changeSize(
                browserId, (float)dpi, (int)std::round(width), (int)std::round(height));
        result(1, nullptr);
    } else if (name.compare("setZoom") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto zoomLevel = webview_value_get_double(webview_value_get_list_value(values, 1));
        m_webviewHandler->setZoomLevel(browserId, zoomLevel);
        result(1, nullptr);
    } else if (
            name.compare("cursorClickDown") == 0 || name.compare("cursorClickUp") == 0 ||
            name.compare("cursorMove") == 0 || name.compare("cursorDragging") == 0) {
        result(cursorAction(name, values), nullptr);
    } else if (name.compare("setScrollDelta") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        CefMouseEvent ev;
        ev.x = webview_value_get_int(webview_value_get_list_value(values, 1));
        ev.y = webview_value_get_int(webview_value_get_list_value(values, 2));
        auto deltaX = webview_value_get_int(webview_value_get_list_value(values, 3));
        auto deltaY = webview_value_get_int(webview_value_get_list_value(values, 4));
        auto isCtrlPressed = webview_value_get_bool(webview_value_get_list_value(values, 5));
        if (isCtrlPressed) {
            ev.modifiers = EVENTFLAG_CONTROL_DOWN;
        }
        m_webviewHandler->sendScrollEvent(browserId, ev, (int)deltaX, (int)deltaY);
        result(1, nullptr);
    } else if (name.compare("goForward") == 0) {
        int browserId = int(webview_value_get_int(values));
        m_webviewHandler->goForward(browserId);
        result(1, nullptr);
    } else if (name.compare("goBack") == 0) {
        int browserId = int(webview_value_get_int(values));
        m_webviewHandler->goBack(browserId);
        result(1, nullptr);
    } else if (name.compare("reload") == 0) {
        int browserId = int(webview_value_get_int(values));
        m_webviewHandler->reload(browserId);
        result(1, nullptr);
    } else if (name.compare("openDevTools") == 0) {
        int browserId = int(webview_value_get_int(values));
        m_webviewHandler->openDevTools(browserId);
        result(1, nullptr);
    } else if (name.compare("imeSetComposition") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto text = webview_value_get_string(webview_value_get_list_value(values, 1));
        m_webviewHandler->imeSetComposition(browserId, text);
        result(1, nullptr);
    } else if (name.compare("imeCommitText") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto text = webview_value_get_string(webview_value_get_list_value(values, 1));
        m_webviewHandler->imeCommitText(browserId, text);
        result(1, nullptr);
    } else if (name.compare("setClientFocus") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        if (m_renderers.find(browserId) != m_renderers.end() && m_renderers[browserId] != nullptr) {
            m_renderers[browserId]->isFocused =
                    webview_value_get_bool(webview_value_get_list_value(values, 1));
            m_webviewHandler->setClientFocus(browserId, m_renderers[browserId]->isFocused);
        }
        result(1, nullptr);
    } else if (name.compare("setCookie") == 0) {
        const auto domain = webview_value_get_string(webview_value_get_list_value(values, 0));
        const auto key = webview_value_get_string(webview_value_get_list_value(values, 1));
        const auto value = webview_value_get_string(webview_value_get_list_value(values, 2));
        m_webviewHandler->setCookie(domain, key, value);
        result(1, nullptr);
    } else if (name.compare("deleteCookie") == 0) {
        const auto domain = webview_value_get_string(webview_value_get_list_value(values, 0));
        const auto key = webview_value_get_string(webview_value_get_list_value(values, 1));
        m_webviewHandler->deleteCookie(domain, key);
        result(1, nullptr);
    } else if (name.compare("visitAllCookies") == 0) {
        m_webviewHandler->visitAllCookies(
                [=](std::map<std::string, std::map<std::string, std::string>> cookies) {
                    WValue* retMap = webview_value_new_map();
                    for (auto& cookie : cookies) {
                        WValue* tempMap = webview_value_new_map();
                        for (auto& c : cookie.second) {
                            WValue* val =
                                    webview_value_new_string(const_cast<char*>(c.second.c_str()));
                            webview_value_set_string(tempMap, c.first.c_str(), val);
                            webview_value_unref(val);
                        }
                        webview_value_set_string(retMap, cookie.first.c_str(), tempMap);
                        webview_value_unref(tempMap);
                    }
                    result(1, retMap);
                    webview_value_unref(retMap);
                });
    } else if (name.compare("visitUrlCookies") == 0) {
        const auto domain = webview_value_get_string(webview_value_get_list_value(values, 0));
        const auto isHttpOnly = webview_value_get_bool(webview_value_get_list_value(values, 1));
        m_webviewHandler->visitUrlCookies(
                domain,
                isHttpOnly,
                [=](std::map<std::string, std::map<std::string, std::string>> cookies) {
                    WValue* retMap = webview_value_new_map();
                    for (auto& cookie : cookies) {
                        WValue* tempMap = webview_value_new_map();
                        for (auto& c : cookie.second) {
                            WValue* val =
                                    webview_value_new_string(const_cast<char*>(c.second.c_str()));
                            webview_value_set_string(tempMap, c.first.c_str(), val);
                            webview_value_unref(val);
                        }
                        webview_value_set_string(retMap, cookie.first.c_str(), tempMap);
                        webview_value_unref(tempMap);
                    }
                    result(1, retMap);
                    webview_value_unref(retMap);
                });
    } else if (name.compare("setJavaScriptChannels") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        WValue* list = webview_value_get_list_value(values, 1);
        auto len = webview_value_get_len(list);
        std::string extensionCode;
        std::vector<std::string> channels;
        for (size_t i = 0; i < len; i++) {
            auto channel = webview_value_get_string(webview_value_get_list_value(list, i));
            channels.push_back(channel);
        }
        m_webviewHandler->setJavaScriptChannels(browserId, channels);
        result(1, nullptr);
    } else if (name.compare("sendJavaScriptChannelCallBack") == 0) {
        const auto error = webview_value_get_bool(webview_value_get_list_value(values, 0));
        const auto ret = webview_value_get_string(webview_value_get_list_value(values, 1));
        const auto callbackId = webview_value_get_string(webview_value_get_list_value(values, 2));
        const auto deleteCallBackFunction =
                webview_value_get_bool(webview_value_get_list_value(values, 3));
        const auto browserId = int(webview_value_get_int(webview_value_get_list_value(values, 4)));
        const auto frameId = webview_value_get_string(webview_value_get_list_value(values, 5));
        m_webviewHandler->sendJavaScriptChannelCallBack(
                error, ret, callbackId, deleteCallBackFunction, browserId, frameId);
        result(1, nullptr);
    } else if (name.compare("executeJavaScript") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto code = webview_value_get_string(webview_value_get_list_value(values, 1));
        m_webviewHandler->executeJavaScript(browserId, code);
        result(1, nullptr);
    } else if (name.compare("evaluateJavascript") == 0) {
        int browserId = int(webview_value_get_int(webview_value_get_list_value(values, 0)));
        const auto code = webview_value_get_string(webview_value_get_list_value(values, 1));
        m_webviewHandler->executeJavaScript(browserId, code, [=](std::string values) {
            WValue* retValue = webview_value_new_string(values.c_str());
            result(1, retValue);
            webview_value_unref(retValue);
        });
    } else {
        result = 0;
    }
}

void WebviewPlugin::sendKeyEvent(CefKeyEvent& ev) {
    m_webviewHandler->sendKeyEvent(ev);
    if (ev.type == KEYEVENT_RAWKEYDOWN && ev.windows_key_code == 0x7B &&
        (ev.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
        for (auto render : m_renderers) {
            if (render.second && render.second->isFocused) {
                m_webviewHandler->openDevTools(render.first);
            }
        }
    }
}

void WebviewPlugin::setInvokeMethodFunc(std::function<void(const std::string&, WValue*)> func) {
    m_invokeFlutterMethod = func;
}

void WebviewPlugin::setCreateTextureFunc(std::function<std::shared_ptr<WebviewTexture>()> func) {
    m_createTextureFunc = func;
}

bool WebviewPlugin::getAnyBrowserFocused() {
    for (auto render : m_renderers) {
        if (render.second && render.second->isFocused) {
            return true;
        }
    }
    return false;
}

void WebviewPlugin::setHandleDialogFunc(OnFileDialogCallback func) {
    m_webviewHandler->onOpenDialogHandler = func;
}

int WebviewPlugin::cursorAction(const std::string& name, WValue* args) {
    if (!args || webview_value_get_len(args) < 4) {
        return 0;
    }
    int browserId = int(webview_value_get_int(webview_value_get_list_value(args, 0)));
    int x = int(webview_value_get_int(webview_value_get_list_value(args, 1)));
    int y = int(webview_value_get_int(webview_value_get_list_value(args, 2)));
    int buttonId = int(webview_value_get_int(webview_value_get_list_value(args, 3)));

    int clickCount = 1;
    if (webview_value_get_len(args) > 4) {
        clickCount = int(webview_value_get_int(webview_value_get_list_value(args, 4)));
        if (clickCount < 1) {
            clickCount = 1;
        }
    }
    if (name.compare("cursorClickDown") == 0) {
        m_webviewHandler->cursorClick(browserId, buttonId, x, y, clickCount, false);
    } else if (name.compare("cursorClickUp") == 0) {
        m_webviewHandler->cursorClick(browserId, buttonId, x, y, clickCount, true);
    } else if (name.compare("cursorMove") == 0) {
        m_webviewHandler->cursorMove(browserId, buttonId, x, y, false);
    } else if (name.compare("cursorDragging") == 0) {
        m_webviewHandler->cursorMove(browserId, buttonId, x, y, true);
    }
    return 1;
}

#ifdef _WIN32
void initCEFProcesses(HINSTANCE hInstance) {
    mainArgs = CefMainArgs(hInstance);
    gWebviewApp = new WebviewApp();
    CefExecuteProcess(mainArgs, gWebviewApp, nullptr);
}
#else
void initCEFProcesses(int argc, char* argv[]) {
#ifdef OS_MAC
    CefScopedLibraryLoader loader;
    if (!loader.LoadInMain()) {
        printf("load cef err");
    }
#endif
    mainArgs = CefMainArgs(argc, argv);
    gWebviewApp = new WebviewApp();
    CefExecuteProcess(mainArgs, gWebviewApp, nullptr);
}
#endif

void startCEF() {
    CefSettings cefs;
    cefs.windowless_rendering_enabled = true;
    cefs.no_sandbox = true;
    if (!userAgent.empty()) {
        CefString(&cefs.user_agent_product) = userAgent;
    }
    CefString(&cefs.locale) = "zh-CN";
#ifdef OS_MAC
    // cef message loop handle by MainApplication on mac
    cefs.external_message_pump = true;
    // CefString(&cefs.browser_subprocess_path) = "/Library/Chaches"; //the helper Program path
#else
    // cef message run in another thread on windows/linux
    cefs.multi_threaded_message_loop = true;
#endif
    if (!localCachePath.empty()) {
        CefString(&cefs.cache_path) = localCachePath;
    }
    CefInitialize(mainArgs, cefs, gWebviewApp.get(), nullptr);
}

void doMessageLoopWork() {
    CefDoMessageLoopWork();
}

void SwapBufferFromBgraToRgba(void* _dest, const void* _src, int width, int height) {
    int32_t* dest = (int32_t*)_dest;
    int32_t* src = (int32_t*)_src;
    int32_t rgba;
    int32_t bgra;
    int length = width * height;
    for (int i = 0; i < length; i++) {
        bgra = src[i];
        // BGRA in hex = 0xAARRGGBB.
        rgba = (bgra & 0x00ff0000) >> 16      // Red >> Blue.
                | (bgra & 0xff00ff00)         // Green Alpha.
                | (bgra & 0x000000ff) << 16;  // Blue >> Red.
        dest[i] = rgba;
    }
}

void stopCEF() {
    CefShutdown();
}
}  // namespace webview_cef
