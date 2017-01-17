#pragma once

#include <glib.h>
#include <functional>

void run_on_main_thread(std::function<bool()>&& f);
void run_on_main_thread_delay(guint seconds, std::function<bool()>&& f);
