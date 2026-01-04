import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import 'webview.dart';

class WebviewManager extends ValueNotifier<bool> {
  static final WebviewManager _instance = WebviewManager._internal();

  factory WebviewManager() => _instance;

  late Completer<void> _creatingCompleter;

  final MethodChannel pluginChannel = const MethodChannel("webview_cef");

  final Map<int, WebViewController> _webViews = <int, WebViewController>{};

  final Map<int, WebViewController> _tempWebViews = <int, WebViewController>{};

  int nextIndex = 1;

  get ready => _creatingCompleter.future;

  WebViewController createWebView(
      {Widget? loading, bool? onlyPrimaryMouseButton}) {
    int browserIndex = nextIndex++;
    final controller = WebViewController(pluginChannel, browserIndex,
        onlyPrimaryMouseButton: onlyPrimaryMouseButton ?? true,
        loading: loading);
    _tempWebViews[browserIndex] = controller;
    return controller;
  }

  void removeWebView(int browserId) {
    if (browserId > 0) {
      _webViews.remove(browserId);
    }
  }

  WebviewManager._internal() : super(false);

  Future<void> initialize({String? userAgent, String? cefCachePath}) async {
    _creatingCompleter = Completer<void>();
    try {
      await pluginChannel.invokeMethod('init', [
        (userAgent != null && userAgent.isNotEmpty) ? userAgent : '',
        (cefCachePath != null && cefCachePath.isNotEmpty) ? cefCachePath : ''
      ]);
      pluginChannel.setMethodCallHandler(methodCallhandler);
      // Wait for the platform to complete initialization.
      await Future.delayed(const Duration(milliseconds: 300));
      _creatingCompleter.complete();
      value = true;
    } on PlatformException catch (e) {
      _creatingCompleter.completeError(e);
    }
    return _creatingCompleter.future;
  }

  @override
  Future<void> dispose() async {
    super.dispose();
    pluginChannel.setMethodCallHandler(null);
    _webViews.clear();
  }

  void onBrowserCreated(int browserIndex, int browserId) {
    _webViews[browserId] = _tempWebViews[browserIndex]!;
    _tempWebViews.remove(browserIndex);
  }

  Future<void> methodCallhandler(MethodCall call) async {
    // std::function<void(int browserId, bool isMainFrame, std::string url, bool userGesture, bool isRedirect)> onBeforeBrowseEvent;
    // std::function<void(int browserId, bool isMainFrame, int transitionType)> onLoadStartEvent;
    // std::function<void(int browserId, bool isMainFrame, int httpStatusCode)> onLoadEndEvent;
    // std::function<void(int browserId, bool isMainFrame, int errorCode)> onLoadErrorEvent;
    switch (call.method) {
      case "onBeforeBrowse":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]?.listener?.onBeforeBrowse?.call(
            call.arguments["isMainFrame"] as bool,
            call.arguments["url"] as String,
            call.arguments["userGesture"] as bool,
            call.arguments["isRedirect"] as bool);
        return;
      case "onLoadStart":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]?.listener?.onLoadStart?.call(
            call.arguments["isMainFrame"] as bool,
            call.arguments["transitionType"] as int);
        return;
      case "onLoadEnd":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]?.listener?.onLoadEnd?.call(
            call.arguments["isMainFrame"] as bool,
            call.arguments["httpStatusCode"] as int);
        return;
      case "onLoadError":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]?.listener?.onLoadError?.call(
            call.arguments["isMainFrame"] as bool,
            call.arguments["errorCode"] as int);
        return;
      case "onUrlChanged":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]
            ?.listener
            ?.onUrlChanged
            ?.call(call.arguments["url"] as String);
        return;
      case "onTitleChanged":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]
            ?.listener
            ?.onTitleChanged
            ?.call(call.arguments["title"] as String);
        return;
      case "onConsoleMessage":
        int browserId = call.arguments["browserId"] as int;
        _webViews[browserId]?.listener?.onConsoleMessage?.call(
            call.arguments["level"] as int,
            call.arguments["message"] as String,
            call.arguments["source"] as String,
            call.arguments["line"] as int);
        return;
      case 'javascriptChannelMessage':
        int browserId = call.arguments['browserId'] as int;
        _webViews[browserId]?.onJavascriptChannelMessage?.call(
            call.arguments['channel'] as String,
            call.arguments['message'] as String,
            call.arguments['callbackId'] as String,
            call.arguments['frameId'] as String);
        return;
      case 'onTooltip':
        int browserId = call.arguments['browserId'] as int;
        _webViews[browserId]?.onToolTip?.call(call.arguments['text'] as String);
        return;
      case 'onCursorChanged':
        int browserId = call.arguments['browserId'] as int;
        _webViews[browserId]
            ?.onCursorChanged
            ?.call(call.arguments['type'] as int);
        return;
      case 'onFocusedNodeChangeMessage':
        int browserId = call.arguments['browserId'] as int;
        bool editable = call.arguments['editable'] as bool;
        _webViews[browserId]?.onFocusedNodeChangeMessage(editable);
        return;
      case 'onImeCompositionRangeChangedMessage':
        int browserId = call.arguments['browserId'] as int;
        _webViews[browserId]
            ?.onImeCompositionRangeChangedMessage
            ?.call(call.arguments['x'] as int, call.arguments['y'] as int);
        return;
      default:
    }
  }

  Future<void> setCookie(String domain, String key, String val) async {
    assert(value);
    await pluginChannel.invokeMethod('setCookie', [domain, key, val]);
  }

  Future<void> deleteCookie(String domain, String key) async {
    assert(value);
    await pluginChannel.invokeMethod('deleteCookie', [domain, key]);
  }

  Future<dynamic> visitAllCookies() {
    assert(value);
    return pluginChannel.invokeMethod('visitAllCookies');
  }

  Future<dynamic> visitUrlCookies(String domain, bool isHttpOnly) {
    assert(value);
    return pluginChannel.invokeMethod('visitUrlCookies', [domain, isHttpOnly]);
  }

  Future<void> quit() async {
    //only call this method when you want to quit the app
    assert(value);
    await pluginChannel.invokeMethod('quit');
  }
}
