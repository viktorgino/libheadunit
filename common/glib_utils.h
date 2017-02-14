#pragma once

#include <glib.h>
#include <functional>

extern GMainContext* run_on_thread_main_context;

void run_on_main_thread(std::function<bool()>&& f);
void run_on_main_thread_delay(guint milliseconds, std::function<bool()>&& f);
