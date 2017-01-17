#include "glib_utils.h"

static gboolean run_on_main_thread_func(gpointer p)
{
    std::function<bool()>* func = reinterpret_cast<std::function<bool()>*>(p);
    bool ret = (*func)();
    delete func;
    return ret ? TRUE : FALSE;
}

void run_on_main_thread(std::function<bool()>&& f)
{
    g_idle_add(&run_on_main_thread_func, new std::function<bool()>(f));
}

void run_on_main_thread_delay(guint seconds, std::function<bool()>&& f)
{
    g_timeout_add_seconds(seconds, &run_on_main_thread_func, new std::function<bool()>(f));
}
