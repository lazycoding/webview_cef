#pragma once
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include "webview_handler.h"
struct DialogCallbackContext {
    GtkWidget* parent;
    FileDialogCallback callback;
    int mode;  // CefDialogHandler::FileDialogMode
    std::string title;
    std::string default_file_path;
    std::vector<std::string> accept_filters;
};

gboolean openDialog(void* data);
