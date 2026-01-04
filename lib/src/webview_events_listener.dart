typedef TitleChangeCb = void Function(String title);
typedef UrlChangeCb = void Function(String url);
/* Log severity levels. from CEF include/internal/cef_types.h
  0:default logging (currently info logging)
  1:verbose logging or debug logging
  2:info logging
  3:warning logging
  4:error logging
  5:fatal logging
  99:disable logging to file for all messages, and to stderr for messages with severity less than fatal
 */
typedef OnConsoleMessage = void Function(
    int level, String message, String source, int line);

class WebviewEventsListener {
  // std::function<void(int browserId, bool isMainFrame, std::string url, bool userGesture, bool isRedirect)> onBeforeBrowseEvent;
  // std::function<void(int browserId, bool isMainFrame, int transitionType)> onLoadStartEvent;
  // std::function<void(int browserId, bool isMainFrame, int httpStatusCode)> onLoadEndEvent;
  // std::function<void(int browserId, bool isMainFrame, int errorCode)> onLoadErrorEvent;
  void Function(
          bool isMainFrame, String url, bool userGesture, bool isRedirect)?
      onBeforeBrowse;
  void Function(bool isMainFrame, int transitionType)? onLoadStart;
  void Function(bool isMainFrame, int httpStatusCode)? onLoadEnd;
  void Function(bool isMainFrame, int errorCode)? onLoadError;
  TitleChangeCb? onTitleChanged;
  UrlChangeCb? onUrlChanged;
  OnConsoleMessage? onConsoleMessage;

  WebviewEventsListener({
    this.onBeforeBrowse,
    this.onLoadStart,
    this.onLoadEnd,
    this.onLoadError,
    this.onTitleChanged,
    this.onUrlChanged,
    this.onConsoleMessage,
  });
}
