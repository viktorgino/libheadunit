#include "mzd_nightmode.h"

#include <dbus/dbus.h>

#define LOGTAG "mazda-nm"

#include "hu_uti.h"

#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

static DBusConnection *service_bus = NULL;

void mzd_nightmode_start() {
    if (service_bus != NULL) return;
    DBusError error;
    dbus_error_init(&error);

    service_bus = dbus_connection_open(SERVICE_BUS_ADDRESS, &error);
    if (!service_bus) {
        loge("DBUS: Failed to connect to SERVICE bus %s: %s", error.name, error.value);
        service_bus = NULL;
        return;
    }

    if (!dbus_bus_register(service_bus, &error)) {
        loge("DBUS: Failed to connect to SERVICE bus %s: %s", error.name, error.value);
        service_bus = NULL;
        return;
    }

    printf("Nightmode service connection established.\n");
}

int mzd_is_night_mode_set() {
    if (service_bus == NULL) return NM_NO_VALUE;

    DBusError error;
    DBusMessage *nm_msg = dbus_message_new_method_call("com.jci.navi2NNG", "/com/jci/navi2NNG", "com.jci.navi2NNG", "GetDayNightMode");
    dbus_message_set_auto_start(nm_msg, TRUE);

    DBusPendingCall* nm_pending;
    dbus_connection_send_with_reply(service_bus, nm_msg, &nm_pending, DBUS_TIMEOUT_INFINITE);
    dbus_connection_flush(service_bus);
    dbus_message_unref(nm_msg);

    dbus_pending_call_block(nm_pending);

    nm_msg = dbus_pending_call_steal_reply(nm_pending);
    dbus_pending_call_unref(nm_pending);

    if (nm_msg) {
        dbus_int32_t nm_daynightmode;
        dbus_error_init(&error);

        if (!dbus_message_get_args(nm_msg, &error, DBUS_TYPE_INT32, &nm_daynightmode, DBUS_TYPE_INVALID)) {
            loge("DBUS: failed to get result %s: %s\n", error.name, error.message);
            dbus_message_unref(nm_msg);
            return NM_NO_VALUE;
        } else {
            dbus_message_unref(nm_msg);
            return (nm_daynightmode == 1) ? NM_NIGHT_MODE : NM_DAY_MODE;
        }
    } else {
        return NM_NO_VALUE;
    }
}

void mzd_nightmode_stop() {
    dbus_connection_unref(service_bus);
    service_bus = NULL;
}