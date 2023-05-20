#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#define BLUEZ_BUS_NAME "org.bluez"
#define OBJECT_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"

static pa_context *pa_ctx = NULL;
static pa_mainloop_api *pa_ml_api = NULL;
static pa_glib_mainloop *pa_gml = NULL;

char *connected_device_address = NULL;
gboolean device_connected = FALSE;
gboolean device_transport_found = FALSE;

GHashTable *known_devices = NULL;  // hash table to keep track of known devices

typedef struct {
	GMainLoop *loop;
	char *device_address;
} CallbackData;

typedef struct {
    guint16 volume;
} VolumeData;


// Callback function for each sink
void sink_info_callback(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
    // If eol is set to a positive number, you're at the end of the list
    if (eol > 0) return;
    
    //g_print("searching for sink\n");

    if (strcmp(i->name, "alsa_output.usb-Focusrite_Scarlett_Solo_USB-00.analog-stereo") == 0) {
        VolumeData *vdata = (VolumeData *)userdata;
        guint16 volume = vdata->volume;
        double scaledVolume = (double)volume / 127.0;
        pa_volume_t pa_volume = pa_sw_volume_from_linear(scaledVolume);
        //g_print("Bluetooth scaled level: %f\n", scaledVolume);
        //g_print("PulseAudio volume level: %u\n", pa_volume);


        // Prepare the pa_cvolume structure
        pa_cvolume cv;
        pa_cvolume_set(&cv, i->volume.channels, pa_volume);

        // Set the volume
        pa_operation *o;
        o = pa_context_set_sink_volume_by_index(c, i->index, &cv, NULL, NULL);
        if (o) {
            pa_operation_unref(o);
        } else {
            g_print("Failed to set volume.\n");
        }
    }
}

void init_pulseaudio() {
    pa_gml = pa_glib_mainloop_new(NULL);
    pa_ml_api = pa_glib_mainloop_get_api(pa_gml);
    pa_ctx = pa_context_new(pa_ml_api, "bluez_volume_control");
    while (pa_context_connect(pa_ctx, NULL, 0, NULL) < 0) {
        pa_gml = pa_glib_mainloop_new(NULL);
        pa_ml_api = pa_glib_mainloop_get_api(pa_gml);
        pa_ctx = pa_context_new(pa_ml_api, "bluez_volume_control");
        g_print("PulseAudio connection failed. Retrying...\n");
        // 1000ms delay
        usleep(1000000);
    }

    g_print("PulseAudio connection successful.\n");
}




static void on_property_changed(GDBusConnection *connection,
                                const gchar *sender_name,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *signal_name,
                                GVariant *parameters,
                                gpointer user_data)
{
    //g_print("Property changed signal received\n");

    CallbackData *data = (CallbackData *)user_data;
    GMainLoop *loop = data->loop;
    gchar* device_address = data->device_address;
    GVariantIter *properties = NULL;
    gchar *iface = NULL;
    GVariant *value = NULL;
    const gchar *key = NULL;

    if (strcmp(interface_name, "org.freedesktop.DBus.Properties") == 0 &&
        strcmp(signal_name, "PropertiesChanged") == 0) {

        g_variant_get(parameters, "(&sa{sv}as)", &iface, &properties, NULL);
        
        if (strcmp(iface, "org.bluez.Device1") == 0) {
            while(g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
                if(!g_strcmp0(key, "Connected")) {
                    if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                        g_print("Invalid argument type for %s: %s != %s", key,
                                g_variant_get_type_string(value), "b");
                        continue;
                    }
                    gboolean is_connected = g_variant_get_boolean(value);
                    g_print("Device: %s, Connection Status: %d\n", device_address, is_connected);
                    if (!is_connected) {
                        device_connected = FALSE;
                        device_transport_found = FALSE;
                        g_main_loop_quit(loop);
                    }
                }
            }
        } else if (strcmp(iface, "org.bluez.MediaTransport1") == 0) {
            while(g_variant_iter_loop(properties, "{&sv}", &key, &value)) {
                if(!g_strcmp0(key, "Volume")) {
                    if(!g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16)) {
                        g_print("Invalid argument type for %s: %s != %s", key,
                                g_variant_get_type_string(value), "q");
                        continue;
                    }
                    guint16 volume = g_variant_get_uint16(value);
                    //g_print("Volume level: %u\n", volume);
                    
                    VolumeData *vdata = g_malloc(sizeof(VolumeData));
                    vdata->volume = volume;
                    
                    pa_context_get_sink_info_list(pa_ctx, sink_info_callback, vdata); 
                }
            }
        }
        
        g_variant_iter_free(properties);
    }
}


static void get_connected_devices(GDBusConnection *conn, GMainLoop *loop) {
    GError *error = NULL;

    GVariant *reply = g_dbus_connection_call_sync(conn,
                                                  BLUEZ_BUS_NAME,
                                                  "/",
                                                  OBJECT_MANAGER_INTERFACE,
                                                  "GetManagedObjects",
                                                  NULL,
                                                  G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  -1,
                                                  NULL,
                                                  &error);

    if (reply) {
        GVariantIter *iter;
        char *key;
        GVariant *value;

        g_variant_get(reply, "(a{oa{sa{sv}}})", &iter);
        while (g_variant_iter_next(iter, "{&o@a{sa{sv}}}", &key, &value)) {
            if (strstr(key, "/dev_")) {
                GVariantIter *inner_iter;
                char *inner_key;
                GVariant *inner_value;

                g_variant_get(value, "a{sa{sv}}", &inner_iter);
                while (g_variant_iter_next(inner_iter, "{&s@a{sv}}", &inner_key, &inner_value)) {
                    if (strcmp(inner_key, "org.bluez.Device1") == 0) {
                        GVariantDict dict;
                        g_variant_dict_init(&dict, inner_value);
                        GVariant *address_variant = g_variant_dict_lookup_value(&dict, "Address", G_VARIANT_TYPE_STRING);
                        GVariant *connected_variant = g_variant_dict_lookup_value(&dict, "Connected", G_VARIANT_TYPE_BOOLEAN);

                        if (address_variant != NULL && connected_variant != NULL) {
                            gboolean is_connected = g_variant_get_boolean(connected_variant);
                            if (is_connected) {
                                g_print("Connected Device1 interface found at: %s\n", key);
                                device_connected = TRUE;
                                const gchar* device_address = g_variant_get_string(address_variant, NULL);
                                if (!g_hash_table_contains(known_devices, device_address)) {
                                    CallbackData *data = g_malloc(sizeof(CallbackData));
                                    if (data) {
                                        data->loop = loop;
                                        data->device_address = g_strdup(device_address);
                                        if (data->device_address) {
                                            g_dbus_connection_signal_subscribe(conn,
                                                "org.bluez",
                                                "org.freedesktop.DBus.Properties",
                                                "PropertiesChanged",
                                                key,
                                                "org.bluez.Device1",
                                                G_DBUS_SIGNAL_FLAGS_NONE,
                                                on_property_changed,
                                                data,
                                                g_free);

                                            g_hash_table_insert(known_devices, g_strdup(data->device_address), NULL);
                                        } else {
                                            g_free(data);
                                        }
                                    }
                                }

                                g_variant_unref(address_variant);
                                g_variant_unref(connected_variant);
                            } else {
                                g_variant_unref(address_variant);
                                g_variant_unref(connected_variant);
                            }
                        }

                        g_variant_dict_clear(&dict);
                    } else if (strcmp(inner_key, "org.bluez.MediaTransport1") == 0) {
                        g_print("MediaTransport1 interface found at: %s\n", key);
                        CallbackData *data = g_malloc(sizeof(CallbackData));
                        if (data) {
                            data->loop = loop;
                            data->device_address = g_strdup(key);
                            if (data->device_address) {
                                device_transport_found = TRUE;
                                g_dbus_connection_signal_subscribe(conn,
                                    "org.bluez",
                                    "org.freedesktop.DBus.Properties",
                                    "PropertiesChanged",
                                    key,
                                    "org.bluez.MediaTransport1",
                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                    on_property_changed,
                                    data,
                                    g_free);
                            } else {
                                g_free(data);
                            }
                        }
                    }
                    g_variant_unref(inner_value);
                }
                g_variant_iter_free(inner_iter);
            }
            g_variant_unref(value);
        }
        g_variant_iter_free(iter);

        g_variant_unref(reply);
    }

    if (error) {
        g_print("Failed to get managed objects: %s\n", error->message);
        g_error_free(error);
    }
}



int main() {
    GDBusConnection *conn;
    GError *error = NULL;
    GMainLoop *loop;
    
    init_pulseaudio();

    known_devices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    while (1) {
        loop = g_main_loop_new(NULL, FALSE);
        conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
        if (conn) {
            // Start listening for device connections.
            do {
                get_connected_devices(conn, loop);
            } while(!device_connected || !device_transport_found);
            
            // Now start the main loop to handle DBus signals.
            g_main_loop_run(loop);
            
            // Clean up when the main loop is done (i.e., when a device disconnects).
            g_free(connected_device_address);
            g_object_unref(conn);
            g_main_loop_unref(loop);
        } else {
            g_print("Failed to connect to system bus: %s\n", error->message);
            g_error_free(error);
            g_main_loop_unref(loop);
            g_hash_table_destroy(known_devices);
            return 1;
        }
    }

    g_hash_table_destroy(known_devices);
    pa_glib_mainloop_free(pa_gml);
    return 0;
}
