#include "glib_utils.h"

GMainContext* run_on_thread_main_context = nullptr;

static gboolean run_on_main_thread_func(gpointer p)
{
    std::function<bool()>* func = reinterpret_cast<std::function<bool()>*>(p);
    bool ret = (*func)();
    delete func;
    return ret ? TRUE : FALSE;
}

void run_on_main_thread(std::function<bool()>&& f)
{
    GSource* source = g_idle_source_new();
    g_source_set_callback(source, run_on_main_thread_func, new std::function<bool()>(f), nullptr);

    g_source_attach(source, run_on_thread_main_context);
    g_source_unref(source);
}

void run_on_main_thread_delay(guint milliseconds, std::function<bool()>&& f)
{
    GSource* source = g_timeout_source_new(milliseconds);
    g_source_set_callback(source, run_on_main_thread_func, new std::function<bool()>(f), nullptr);

    g_source_attach(source, run_on_thread_main_context);
    g_source_unref(source);
}
