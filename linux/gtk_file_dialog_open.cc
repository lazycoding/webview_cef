#include "gtk_file_dialog_open.h"
#include <gtk/gtk.h>
#include <string>
#include <vector>

static GtkFileChooserNative* create_dialog(
        GtkWindow* window,
        GtkFileChooserAction action,
        const std::string& title,
        const std::string& default_file_path,
        const std::string& default_confirm_button_text,
        const std::vector<std::string>& accept_filters,
        gboolean multiple) {
    g_autoptr(GtkFileChooserNative) dialog = GTK_FILE_CHOOSER_NATIVE(gtk_file_chooser_native_new(
            title.c_str(), window, action, default_confirm_button_text.c_str(), "_Cancel"));

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), multiple);

    if (!default_file_path.empty() &&
        (action == GTK_FILE_CHOOSER_ACTION_SAVE ||
         action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)) {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_file_path.c_str());
    }

    if (!accept_filters.empty()) {
        g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "自定义文件类型");

        for (const auto& accept_filter : accept_filters) {
            std::string filter_str = accept_filter;
            if (filter_str.find('|') != std::string::npos) {
                gtk_file_filter_add_pattern(filter, filter_str.c_str());
            } else if (filter_str.find('.') == 0) {
                std::string pattern_str;
                pattern_str = "*";
                pattern_str += filter_str;
                gtk_file_filter_add_pattern(filter, pattern_str.c_str());
            } else {
                gtk_file_filter_add_pattern(filter, filter_str.c_str());
            }
        }

        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    }

    return GTK_FILE_CHOOSER_NATIVE(g_object_ref(dialog));
}

static GtkFileChooserNative* create_dialog_for_method(
        GtkWindow* window,
        int mode,
        const std::string& title,
        const std::string& default_file_path,
        const std::vector<std::string>& accept_filters) {
    if (mode == CefDialogHandler::FileDialogMode::FILE_DIALOG_OPEN ||
        mode == CefDialogHandler::FileDialogMode::FILE_DIALOG_OPEN_MULTIPLE) {
        return create_dialog(
                window,
                GTK_FILE_CHOOSER_ACTION_OPEN,
                title.empty() ? "OPEN" : title,
                default_file_path,
                "_Open",
                accept_filters,
                mode == CefDialogHandler::FileDialogMode::FILE_DIALOG_OPEN_MULTIPLE);
    } else if (mode == CefDialogHandler::FileDialogMode::FILE_DIALOG_OPEN_FOLDER) {
        return create_dialog(
                window,
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                title.empty() ? "OPEN FOLDER" : title,
                default_file_path,
                "_Open",
                accept_filters,
                FALSE);
    } else if (mode == CefDialogHandler::FileDialogMode::FILE_DIALOG_SAVE) {
        return create_dialog(
                window,
                GTK_FILE_CHOOSER_ACTION_SAVE,
                title.empty() ? "SAVE" : title,
                default_file_path,
                "_Save",
                accept_filters,
                FALSE);
    }
    return nullptr;
}

template <class T>
class ScopeGuard {
public:
    ScopeGuard(T* p) : ptr(p) {}
    ~ScopeGuard() {
        if (ptr) {
            delete ptr;
        }
    }

private:
    T* ptr;
};

gboolean openDialog(void* data) {
    DialogCallbackContext* context = (DialogCallbackContext*)data;
    if (context == NULL) {
        return FALSE;
    }

    ScopeGuard<DialogCallbackContext> guard(context);

    g_autoptr(GtkFileChooserNative) dialog = create_dialog_for_method(
            GTK_WINDOW(context->parent),
            context->mode,
            context->title,
            context->default_file_path,
            context->accept_filters);
    if (dialog == nullptr) {
        return FALSE;
    }
    gint response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog));
    std::vector<std::string> return_list;
    bool success = false;
    if (response == GTK_RESPONSE_ACCEPT) {
        g_autoptr(GSList) filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        for (GSList* link = filenames; link != nullptr; link = link->next) {
            gchar* filename = static_cast<gchar*>(link->data);
            return_list.push_back(filename);
        }
        success = true;
    }
    if (context->callback) {
        context->callback(success, return_list);
    }

    gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(dialog));
    return FALSE;
}