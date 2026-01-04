import 'dart:async';
import 'dart:io';

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'webview_manager.dart';
import 'webview_events_listener.dart';
import 'webview_javascript.dart';
import 'webview_textinput.dart';
import 'webview_tooltip.dart';

/// 业务上需要根据 index 进行阶段判断。优化状态时需要确保顺序或对判断机制优化
enum WebViewStatus {
  create,
  init,
  load,
}

class WebViewController extends ValueNotifier<WebViewStatus> {
  WebViewController(this._pluginChannel, this._index,
      {Widget? loading, bool onlyPrimaryMouseButton = true})
      : super(WebViewStatus.create) {
    _loadingWidget = loading;
    _onlyPrimaryMouseButton = onlyPrimaryMouseButton;
  }
  final MethodChannel _pluginChannel;
  Widget? _loadingWidget;

  late WebView _webviewWidget;
  Widget get webviewWidget => _webviewWidget;
  Widget get loadingWidget => _loadingWidget ?? const Text("loading...");

  late Completer<void> _creatingCompleter;
  Future<void> get ready => _creatingCompleter.future;
  bool _isDisposed = false;
  bool _focusEditable = true;
  late bool _onlyPrimaryMouseButton;

  final int _index;
  late int _browserId;
  late int _textureId;
  final Map<String, JavascriptChannel> _javascriptChannels =
      <String, JavascriptChannel>{};
  Map<String, JavascriptChannel> get javascriptChannels => _javascriptChannels;
  WebviewEventsListener? _listener;
  WebviewEventsListener? get listener => _listener;

  get onJavascriptChannelMessage => (final String channelName,
          final String message, final String callbackId, final String frameId) {
        if (_javascriptChannels.containsKey(channelName)) {
          _javascriptChannels[channelName]!.onMessageReceived(
              JavascriptMessage(message, callbackId, frameId));
        } else {
          throw Exception('Channel "$channelName" is not exstis');
        }
      };

  get onToolTip => _onToolTip;
  get onCursorChanged => _onCursorChanged;
  get onFocusedNodeChangeMessage => _onFocusedNodeChangeMessage;
  get onImeCompositionRangeChangedMessage =>
      _onImeCompositionRangeChangedMessage;

  /// Initializes the underlying platform view.
  Future<WebViewController?> initialize(
      {double zoomLevel = 0.0,
      TextStyle? tooltipTextStyle,
      List<String>? forbiddenUrls}) async {
    if (_isDisposed) {
      return null;
    }
    _creatingCompleter = Completer<void>();
    try {
      await WebviewManager().ready;
      // create args: [url, zoom]
      List args = await _pluginChannel.invokeMethod('create', [
        '',
        zoomLevel,
        forbiddenUrls != null ? forbiddenUrls.join(',') : '',
      ]);
      _browserId = args[0] as int;
      _textureId = args[1] as int;
      WebviewManager().onBrowserCreated(_index, _browserId);
      await Future.delayed(const Duration(milliseconds: 50));
      _webviewWidget = WebView(this, tooltipTextStyle: tooltipTextStyle);
      value = WebViewStatus.init;
      _creatingCompleter.complete();
    } on PlatformException catch (e) {
      _creatingCompleter.completeError(e);
    }
    await _creatingCompleter.future;
    return this;
  }

  setWebviewListener(WebviewEventsListener listener) {
    _listener = listener;
  }

  final FocusNode _focusNode = FocusScopeNode(debugLabel: "webview_cef");
  FocusNode get focusNode => _focusNode;

  void requestFocus() {
    _focusNode.requestFocus();
  }

  void unfocus() {
    _focusNode.unfocus();
  }

  @override
  Future<void> dispose() async {
    await _creatingCompleter.future;
    if (!_isDisposed) {
      _isDisposed = true;
      WebviewManager().removeWebView(_browserId);
      await _pluginChannel.invokeMethod('close', _browserId);
    }
    super.dispose();
  }

  /// 设置浏览器缩放比例，默认为0
  Future<void> setZoom(double level) async {
    if (_isDisposed) {
      return;
    }
    if (value.index >= WebViewStatus.init.index) {
      return _pluginChannel.invokeMethod('setZoom', [_browserId, level]);
    }
    return;
  }

  /// Loads the given [url].
  Future<void> loadUrl(String url) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    await _pluginChannel.invokeMethod('loadUrl', [_browserId, url]);
    if (value == WebViewStatus.init) {
      // 延迟进入 load 状态，防止页面首次加载时先黑屏或者飘红异常再显示
      Future.delayed(const Duration(milliseconds: 1000), () {
        value = WebViewStatus.load;
      });
    }
  }

  /// Reloads the current document.
  Future<void> reload() async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('reload', _browserId);
  }

  Future<void> goForward() async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('goForward', _browserId);
  }

  Future<void> goBack() async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('goBack', _browserId);
  }

  Future<void> openDevTools() async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('openDevTools', _browserId);
  }

  Future<void> imeSetComposition(String composingText) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel
        .invokeMethod('imeSetComposition', [_browserId, composingText]);
  }

  Future<void> imeCommitText(String composingText) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    // print("@@@imeCommitText: browser=$_browserId, text=$composingText");
    return _pluginChannel
        .invokeMethod('imeCommitText', [_browserId, composingText]);
  }

  Future<void> setClientFocus(bool focus) async {
    if (_isDisposed) {
      return;
    }
    if (value.index >= WebViewStatus.init.index) {
      return _pluginChannel.invokeMethod('setClientFocus', [_browserId, focus]);
    }
    return;
  }

  Future<void> setJavaScriptCode(String code) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('injectJs', code);
  }

  Future<void> setJavaScriptChannels(Set<JavascriptChannel> channels) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    _assertJavascriptChannelNamesAreUnique(channels);

    for (var channel in channels) {
      _javascriptChannels[channel.name] = channel;
    }

    return _pluginChannel.invokeMethod('setJavaScriptChannels',
        [_browserId, _extractJavascriptChannelNames(channels).toList()]);
  }

  void addJavaScriptChannels(Set<JavascriptChannel> channels) {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    _assertJavascriptChannelNamesAreUnique(channels);

    for (var channel in channels) {
      _javascriptChannels[channel.name] = channel;
    }
  }

  /// 默认情况下一个 callbackId 只能处理一次回调，后续回调会被忽略。
  ///
  /// 如果需要通知进度，最后完成时再删除，可以使用 [deleteCallBackFunction] 来手动管理，
  /// 过程中的通知 [deleteCallBackFunction] 传 false，最后完成时 [sendJavaScriptChannelCallBack] 传 true
  Future<void> sendJavaScriptChannelCallBack(
    bool error,
    String result,
    String callbackId,
    String frameId, {
    bool deleteCallBackFunction = true,
  }) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('sendJavaScriptChannelCallBack', [
      error,
      result,
      callbackId,
      deleteCallBackFunction,
      _browserId,
      frameId
    ]);
  }

  Future<void> executeJavaScript(String code) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('executeJavaScript', [_browserId, code]);
  }

  Future<dynamic> evaluateJavascript(String code) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel
        .invokeMethod('evaluateJavascript', [_browserId, code]);
  }

  /// Moves the virtual cursor to [position].
  Future<void> _cursorMove(int buttonId, Offset position) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('cursorMove',
        [_browserId, position.dx.round(), position.dy.round(), buttonId]);
  }

  Future<void> _cursorDragging(int buttonId, Offset position) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('cursorDragging',
        [_browserId, position.dx.round(), position.dy.round(), buttonId]);
  }

  Future<void> _cursorClickDown(int buttonId, Offset position) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    if (buttonId != kPrimaryMouseButton && _onlyPrimaryMouseButton) {
      return;
    }
    return _pluginChannel.invokeMethod('cursorClickDown',
        [_browserId, position.dx.round(), position.dy.round(), buttonId, 1]);
  }

  Future<void> _cursorDoubleClick(int buttonId, Offset position) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    if (buttonId != kPrimaryMouseButton && _onlyPrimaryMouseButton) {
      return;
    }
    return _pluginChannel.invokeMethod('cursorClickDown',
        [_browserId, position.dx.round(), position.dy.round(), buttonId, 2]);
  }

  Future<void> _cursorClickUp(int buttonId, Offset position) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    if (buttonId != kPrimaryMouseButton && _onlyPrimaryMouseButton) {
      return;
    }
    return _pluginChannel.invokeMethod('cursorClickUp',
        [_browserId, position.dx.round(), position.dy.round(), buttonId]);
  }

  /// Sets the horizontal and vertical scroll delta.
  Future<void> _setScrollDelta(
      Offset position, int dx, int dy, bool isCtrlPressed) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel.invokeMethod('setScrollDelta', [
      _browserId,
      position.dx.round(),
      position.dy.round(),
      dx,
      dy,
      isCtrlPressed
    ]);
  }

  /// Sets the surface size to the provided [size].
  Future<void> _setSize(double dpi, Size size) async {
    if (_isDisposed) {
      return;
    }
    assert(value.index >= WebViewStatus.init.index);
    return _pluginChannel
        .invokeMethod('setSize', [_browserId, dpi, size.width, size.height]);
  }

  Set<String> _extractJavascriptChannelNames(Set<JavascriptChannel> channels) {
    final Set<String> channelNames =
        channels.map((JavascriptChannel channel) => channel.name).toSet();
    return channelNames;
  }

  void _assertJavascriptChannelNamesAreUnique(
      final Set<JavascriptChannel>? channels) {
    if (channels == null || channels.isEmpty) {
      return;
    }
    assert(_extractJavascriptChannelNames(channels).length == channels.length);
  }

  /// cef tooltip handler
  Function(String)? _onToolTip;

  /// cef cursor changed handler
  Function(int)? _onCursorChanged;

  /// cef focus changed handler
  Function(bool editable)? _onFocusedNodeChangeMessage;

  /// cef ime composition range changed handler
  Function(int, int)? _onImeCompositionRangeChangedMessage;
}

class WebView extends StatefulWidget {
  final WebViewController controller;
  final TextStyle? tooltipTextStyle;

  const WebView(this.controller, {Key? key, this.tooltipTextStyle})
      : super(key: key);

  @override
  WebViewState createState() => WebViewState();
}

class WebViewState extends State<WebView> with WebViewTextInput {
  final GlobalKey _key = GlobalKey();

  bool isPrimaryFocus = false;
  WebviewTooltip? _tooltip;
  MouseCursor _mouseType = SystemMouseCursors.basic;

  WebViewController get _controller => widget.controller;

  TextEditingValue? _textEditingValue;

  @override
  TextEditingValue? get currentTextEditingValue => _textEditingValue;

  String _composingText = '';
  @override
  updateEditingValueWithDeltas(List<TextEditingDelta> textEditingDeltas) {
    /// Handles IME composition only
    for (var d in textEditingDeltas) {
      if (d is TextEditingDeltaInsertion) {
        // composing text
        if (d.composing.isValid) {
          _composingText += d.textInserted;
          _controller.imeSetComposition(_composingText);
        } else if (!Platform.isWindows) {
          _controller.imeCommitText(d.textInserted);
        }
      } else if (d is TextEditingDeltaDeletion) {
        if (d.composing.isValid) {
          if (_composingText == d.textDeleted) {
            _composingText = "";
          }
          _controller.imeSetComposition(_composingText);
        }
      } else if (d is TextEditingDeltaReplacement) {
        if (d.composing.isValid) {
          _composingText = d.replacementText;
          _controller.imeSetComposition(_composingText);
        }
      } else if (d is TextEditingDeltaNonTextUpdate) {
        if (_composingText.isNotEmpty) {
          _controller.imeCommitText(_composingText);
          _composingText = '';
        }
      }
    }
  }

  @override
  void initState() {
    super.initState();
    _controller._onFocusedNodeChangeMessage = (editable) {
      debugPrint("_onFocusedNodeChangeMessage:editable=$editable");
      _composingText = '';
      _controller._focusEditable = editable;
      if (_controller.focusNode.hasFocus) {
        if (editable) {
          attachTextInputClient();
        } else {
          detachTextInputClient();
        }
      } else {
        _controller.focusNode.requestFocus();
      }
      setState(() {});
    };

    _controller._onImeCompositionRangeChangedMessage = (x, y) {
      final box = _key.currentContext!.findRenderObject() as RenderBox;
      updateIMEComposionPosition(
          x.toDouble(), y.toDouble(), box.localToGlobal(Offset.zero));
    };

    _controller._onToolTip = (final String text) {
      _tooltip ??= WebviewTooltip(_key.currentContext!,
          textStyle: widget.tooltipTextStyle);
      _tooltip!.showToolTip(text);
    };

    _controller._onCursorChanged = (int type) {
      switch (type) {
        case 0:
          _mouseType = SystemMouseCursors.basic;
          break;
        case 1:
          _mouseType = SystemMouseCursors.precise;
          break;
        case 2:
          _mouseType = SystemMouseCursors.click;
          break;
        case 3:
          _mouseType = SystemMouseCursors.text;
          break;
        case 4:
          _mouseType = SystemMouseCursors.wait;
          break;
        default:
          _mouseType = SystemMouseCursors.basic;
          break;
      }
      setState(() {});
    };

    // Report initial surface size
    WidgetsBinding.instance
        .addPostFrameCallback((_) => _reportSurfaceSize(context));
  }

  @override
  Widget build(BuildContext context) {
    return Focus.withExternalFocusNode(
      autofocus: true,
      focusNode: _controller.focusNode,
      onFocusChange: (focused) {
        _composingText = '';
        debugPrint(
            "onFocusChange: focused=$focused, focusEditable=${_controller._focusEditable}");
        if (focused != true) {
          //失去焦点时，隐藏tip提醒框
          _tooltip?.showToolTip('');
        }
        _textEditingValue = TextEditingValue.empty;
        updateTextEditingValue();
        _controller.setClientFocus(focused);
        if (_controller._focusEditable) {
          focused ? attachTextInputClient() : detachTextInputClient();
        } else {
          _textEditingValue = TextEditingValue.empty;
        }
      },
      child: SizedBox.expand(key: _key, child: _buildInner()),
    );
  }

  int _buttonPressed = 0;

  Widget _buildInner() {
    return NotificationListener<SizeChangedLayoutNotification>(
      onNotification: (notification) {
        _reportSurfaceSize(context);
        return true;
      },
      child: SizeChangedLayoutNotifier(
        child: Listener(
          onPointerHover: (ev) {
            _controller._cursorMove(ev.buttons, ev.localPosition);
            _tooltip?.cursorOffset = ev.localPosition;
          },
          onPointerDown: _handleClickDown,
          onPointerUp: _handleClickUp,
          onPointerMove: (ev) {
            if (ev.buttons == kPrimaryMouseButton) {
              _controller._cursorDragging(ev.buttons, ev.localPosition);
            }
          },
          onPointerSignal: (signal) {
            if (signal is PointerScrollEvent) {
              final pressed = HardwareKeyboard.instance.logicalKeysPressed;
              bool isCtrlPressed =
                  (pressed.contains(LogicalKeyboardKey.controlLeft) ||
                      pressed.contains(LogicalKeyboardKey.controlRight));
              _controller._setScrollDelta(
                  signal.localPosition,
                  signal.scrollDelta.dx.round(),
                  (signal.scrollDelta.dy / 5).round(),
                  isCtrlPressed);
            }
          },
          onPointerPanZoomUpdate: (event) {
            _controller._setScrollDelta(event.localPosition,
                event.panDelta.dx.round(), event.panDelta.dy.round(), false);
          },
          child: MouseRegion(
            cursor: _mouseType,
            child: Texture(textureId: _controller._textureId),
          ),
        ),
      ),
    );
  }

  DateTime? _lastClickTime;

  void _handleClickDown(PointerDownEvent ev) {
    if (_lastClickTime != null &&
        DateTime.now().difference(_lastClickTime!) <
            const Duration(milliseconds: 300)) {
      _buttonPressed = ev.buttons;
      _controller._cursorDoubleClick(ev.buttons, ev.localPosition);
      _lastClickTime = DateTime.now();
      // Linux special case: refer to WebeViewTextInput.updateIMEComposionPosition
      if (Platform.isLinux) {
        Future.delayed(const Duration(milliseconds: 150), () {
          _controller._cursorClickDown(ev.buttons, ev.localPosition);
        });
      }
      _lastClickTime = null;
      return;
    }

    if (!_controller.focusNode.hasFocus) {
      _controller._onImeCompositionRangeChangedMessage?.call(0, 0);
      _controller.focusNode.requestFocus();
    }
    _buttonPressed = ev.buttons;
    _controller._cursorClickDown(ev.buttons, ev.localPosition);
    _lastClickTime = DateTime.now();
  }

  void _handleClickUp(PointerUpEvent ev) {
    // flutter issue: https://github.com/flutter/flutter/issues/29888
    PointerUpEvent event = ev.copyWith(buttons: _buttonPressed);
    _controller._cursorClickUp(event.buttons, event.localPosition);
  }

  void _reportSurfaceSize(BuildContext context) async {
    double dpi = MediaQuery.of(context).devicePixelRatio;
    final box = _key.currentContext?.findRenderObject() as RenderBox?;
    if (box != null) {
      await _controller.ready;
      unawaited(
          _controller._setSize(dpi, Size(box.size.width, box.size.height)));
    }
  }
}
