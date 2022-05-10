#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "dac.h"
#include "usb.h"
#include "midi_message.h"
#include "usb_midi.h"
#include "launchpad.h"
#include "channel.h"


#define NUM_CHANNELS 4


static const char *TAG = "espmidi";


const channel_config_t channel_configs[NUM_CHANNELS] = {
    {
        .gate_pin = GPIO_NUM_6,
        .trigger_pin = GPIO_NUM_7,
        .dac_pin = GPIO_NUM_1
    },
    {
        .gate_pin = GPIO_NUM_8,
        .trigger_pin = GPIO_NUM_9,
        .dac_pin = GPIO_NUM_2
    },
    {
        .gate_pin = GPIO_NUM_10,
        .trigger_pin = GPIO_NUM_11,
        .dac_pin = GPIO_NUM_3
    },
    {
        .gate_pin = GPIO_NUM_12,
        .trigger_pin = GPIO_NUM_13,
        .dac_pin = GPIO_NUM_34
    }
};

static usb_midi_t usb_midi;
static launchpad_t launchpad;
static channel_t channels[NUM_CHANNELS];


void usb_midi_connected_callback(const usb_device_desc_t *desc) {
    if (desc->idVendor == LAUNCHPAD_VENDOR_ID && desc->idProduct == LAUNCHPAD_PRO_PRODUCT_ID) {
        launchpad_connected_callback(&launchpad, desc);
    }
}

void usb_midi_disconnected_callback(const usb_device_desc_t *desc) {
    if (launchpad.connected) {
        launchpad_disconnected_callback(&launchpad);
    }
}

void usb_midi_recv_callback(const midi_message_t *message) {
    if (launchpad.connected) {
        launchpad_recv_callback(&launchpad, message);
    }

    if (message->command == MIDI_COMMAND_NOTE_ON) {
        channel_set_note(&channels[0], message->note_on.note);
        channel_set_velocity(&channels[0], message->note_on.velocity);
        channel_set_gate(&channels[0], true);
    } else if (message->command == MIDI_COMMAND_NOTE_OFF && channels[0].note == message->note_off.note) {
        channel_set_velocity(&channels[0], 0);
        channel_set_gate(&channels[0], false);
    }
}


void app_main(void) {
    // initialize the dac
    ESP_ERROR_CHECK(dac_global_init());

    // initialize the usb interface
    const usb_midi_config_t usb_midi_config = {
        .callbacks = {
            .connected = usb_midi_connected_callback,
            .disconnected = usb_midi_disconnected_callback,
            .recv = usb_midi_recv_callback
        }
    };
    ESP_ERROR_CHECK(usb_midi_init(&usb_midi_config, &usb_midi));
    ESP_ERROR_CHECK(usb_init(&usb_midi.driver_config));

    // initialize the launchpad controller
    const launchpad_config_t launchpad_config = {
        .usb_midi = &usb_midi
    };
    ESP_ERROR_CHECK(launchpad_init(&launchpad_config, &launchpad));

    // initialize all output channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ESP_ERROR_CHECK(channel_init(&channel_configs[i], &channels[i]));
    }

    // app is now running, we can delete the setup task
    vTaskDelete(NULL);
}
