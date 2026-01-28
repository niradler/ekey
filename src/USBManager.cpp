#include "USBManager.h"
#include <hid_usage_keyboard.h>

// Static member initialization
KeyboardReportCallback USBManager::_keyboardCb = nullptr;
bool USBManager::_initialized = false;
TaskHandle_t USBManager::_usbLibTaskHandle = nullptr;
TaskHandle_t USBManager::_hidHostTaskHandle = nullptr;
volatile bool USBManager::_tasksShouldExit = false;

static QueueHandle_t hid_host_event_queue = nullptr;

typedef struct {
    hid_host_device_handle_t hid_device_handle;
    hid_host_driver_event_t event;
    void *arg;
} hid_host_event_queue_t;

static const char *hid_proto_name_str[] = {"NONE", "KEYBOARD", "MOUSE"};

void USBManager::begin() {
    if (_initialized) {
        Serial.println("[USB] Already initialized");
        return;
    }

    _tasksShouldExit = false;

    Serial.println("[USB] Installing USB Host library...");
    BaseType_t task_created = xTaskCreatePinnedToCore(
        usb_lib_task, "usb_events", 4096,
        xTaskGetCurrentTaskHandle(), 2,
        &_usbLibTaskHandle, 0
    );

    if (task_created != pdTRUE) {
        Serial.println("[USB] ERROR: Failed to create USB lib task!");
        return;
    }

    // Wait for USB host to be ready (with timeout)
    if (ulTaskNotifyTake(false, pdMS_TO_TICKS(2000)) == 0) {
        Serial.println("[USB] WARNING: USB Host init timeout");
    }
    Serial.println("[USB] USB Host library ready");

    Serial.println("[USB] Installing HID driver...");
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    esp_err_t err = hid_host_install(&hid_host_driver_config);
    if (err != ESP_OK) {
        Serial.printf("[USB] ERROR: HID install failed: %s\n", esp_err_to_name(err));
        // Cleanup the USB lib task we started
        if (_usbLibTaskHandle) {
            vTaskDelete(_usbLibTaskHandle);
            _usbLibTaskHandle = nullptr;
        }
        return;
    }

    task_created = xTaskCreate(
        &hid_host_task, "hid_task", 4096, NULL, 2,
        &_hidHostTaskHandle
    );

    if (task_created != pdTRUE) {
        Serial.println("[USB] ERROR: Failed to create HID host task!");
        hid_host_uninstall();
        if (_usbLibTaskHandle) {
            vTaskDelete(_usbLibTaskHandle);
            _usbLibTaskHandle = nullptr;
        }
        return;
    }

    _initialized = true;
    Serial.println("[USB] HID driver ready");
}

void USBManager::end() {
    if (!_initialized) {
        Serial.println("[USB] Not initialized, skipping cleanup");
        return;
    }

    Serial.println("[USB] Beginning cleanup...");

    // 1. Signal tasks to exit their loops
    _tasksShouldExit = true;

    // 2. Free all USB devices
    Serial.println("[USB] Freeing USB devices...");
    usb_host_device_free_all();
    delay(100);

    // 3. Uninstall HID driver (this also stops HID background task created by driver)
    Serial.println("[USB] Uninstalling HID driver...");
    esp_err_t err = hid_host_uninstall();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[USB] HID uninstall warning: %s\n", esp_err_to_name(err));
    }

    // 4. Wait for tasks to see exit flag and suspend
    delay(200);

    // 5. Delete HID host task
    if (_hidHostTaskHandle) {
        eTaskState state = eTaskGetState(_hidHostTaskHandle);
        if (state != eDeleted) {
            vTaskDelete(_hidHostTaskHandle);
        }
        _hidHostTaskHandle = nullptr;
        Serial.println("[USB] HID task deleted");
    }

    // 6. Delete event queue
    if (hid_host_event_queue) {
        vQueueDelete(hid_host_event_queue);
        hid_host_event_queue = nullptr;
        Serial.println("[USB] Event queue deleted");
    }

    // 7. Uninstall USB host library
    Serial.println("[USB] Uninstalling USB host library...");
    err = usb_host_uninstall();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[USB] USB host uninstall warning: %s\n", esp_err_to_name(err));
    }

    // 8. Delete USB lib task
    if (_usbLibTaskHandle) {
        eTaskState state = eTaskGetState(_usbLibTaskHandle);
        if (state != eDeleted) {
            vTaskDelete(_usbLibTaskHandle);
        }
        _usbLibTaskHandle = nullptr;
        Serial.println("[USB] USB lib task deleted");
    }

    // 9. Reset state
    _keyboardCb = nullptr;
    _initialized = false;
    _tasksShouldExit = false;

    Serial.println("[USB] Cleanup complete");
}

void USBManager::usb_lib_task(void *arg) {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        Serial.printf("[USB] ERROR: usb_host_install failed: %s\n", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    // Notify caller that USB host is ready
    xTaskNotifyGive((TaskHandle_t)arg);

    // Event loop with exit check
    while (!_tasksShouldExit) {
        uint32_t event_flags;
        // Use timeout instead of portMAX_DELAY to check exit flag periodically
        err = usb_host_lib_handle_events(pdMS_TO_TICKS(100), &event_flags);

        if (err == ESP_OK && (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)) {
            usb_host_device_free_all();
        }
    }

    // Suspend before being deleted by end()
    vTaskSuspend(NULL);
}

void USBManager::hid_host_task(void *pvParameters) {
    hid_host_event_queue_t evt_queue;
    hid_host_event_queue = xQueueCreate(10, sizeof(hid_host_event_queue_t));

    if (!hid_host_event_queue) {
        Serial.println("[USB] ERROR: Failed to create event queue!");
        vTaskDelete(NULL);
        return;
    }

    // Event loop with exit check
    while (!_tasksShouldExit) {
        if (xQueueReceive(hid_host_event_queue, &evt_queue, pdMS_TO_TICKS(50))) {
            hid_host_device_event(evt_queue.hid_device_handle, evt_queue.event,
                                  evt_queue.arg);
        }
    }

    // Suspend before being deleted by end()
    vTaskSuspend(NULL);
}

void USBManager::hid_host_device_callback(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_driver_event_t event, void *arg) {

    // Safety check - queue might not exist during cleanup
    if (!hid_host_event_queue) return;

    const hid_host_event_queue_t evt_queue = {
        .hid_device_handle = hid_device_handle,
        .event = event,
        .arg = arg
    };
    xQueueSend(hid_host_event_queue, &evt_queue, 0);
}

void USBManager::hid_host_device_event(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_driver_event_t event, void *arg) {

    hid_host_dev_params_t dev_params;
    if (hid_host_device_get_params(hid_device_handle, &dev_params) != ESP_OK) {
        return;
    }

    const hid_host_device_config_t dev_config = {
        .callback = hid_host_interface_callback,
        .callback_arg = NULL
    };

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        Serial.printf("[USB] %s connected!\n", hid_proto_name_str[dev_params.proto]);

        // Skip NONE protocol devices to save hardware channels (max 8 on ESP32-S3)
        if (dev_params.proto == HID_PROTOCOL_NONE) {
            Serial.println("[USB] Skipping NONE protocol device to save channels");
            break;
        }

        if (hid_host_device_open(hid_device_handle, &dev_config) != ESP_OK) {
            Serial.println("[USB] Failed to open HID device");
            break;
        }

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT);
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                hid_class_request_set_idle(hid_device_handle, 0, 0);
            }
        }

        if (hid_host_device_start(hid_device_handle) != ESP_OK) {
            Serial.println("[USB] Failed to start HID device");
        }
        break;

    default:
        break;
    }
}

void USBManager::hid_host_interface_callback(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_interface_event_t event, void *arg) {

    uint8_t data[64] = {0};
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;

    if (hid_host_device_get_params(hid_device_handle, &dev_params) != ESP_OK) {
        return;
    }

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        if (hid_host_device_get_raw_input_report_data(hid_device_handle, data, 64,
                                                      &data_length) == ESP_OK) {
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class &&
                HID_PROTOCOL_KEYBOARD == dev_params.proto &&
                _keyboardCb) {
                _keyboardCb(data, data_length);
            }
        }
        break;

    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        Serial.printf("[USB] %s disconnected\n", hid_proto_name_str[dev_params.proto]);
        hid_host_device_close(hid_device_handle);
        break;

    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        Serial.printf("[USB] %s transfer error\n", hid_proto_name_str[dev_params.proto]);
        break;

    default:
        break;
    }
}
