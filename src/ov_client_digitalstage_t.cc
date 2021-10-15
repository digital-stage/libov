/*
 * This file is part of the ovbox software tool, see <http://orlandoviols.com/>.
 *
 * Copyright (c) 2020 Giso Grimm, Tobias Hegemann
 */
/*
 * ovbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * ovbox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with ovbox / dsbox. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "ov_client_digitalstage_t.h"

#include <utility>

using namespace DigitalStage::Api;
using namespace DigitalStage::Types;

ov_client_digitalstage_t::ov_client_digitalstage_t(ov_render_base_t &backend,
                                                   std::string apiUrl_,
                                                   std::string apiKey_)
        : ov_client_base_t(backend), apiKey(std::move(apiKey_)), apiUrl(std::move(apiUrl_)), shouldQuit(false) {
}

void ov_client_digitalstage_t::start_service() {
    client = std::make_unique<DigitalStage::Api::Client>(apiUrl);
    controller = std::make_unique<ov_ds_sockethandler_t>(&backend, client.get());
    controller->enable();

    client->ready.connect(&ov_client_digitalstage_t::onReady, this);

    // Send initial device
    nlohmann::json initialDevice;
    initialDevice["uuid"] = backend.get_deviceid();
    initialDevice["type"] = "ov";
    initialDevice["canOv"] = true;
    initialDevice["canAudio"] = true;
    initialDevice["canVideo"] = false;
    initialDevice["sendAudio"] = true;
    initialDevice["receiveAudio"] = true;
    client->connect(apiKey, initialDevice);
}

void ov_client_digitalstage_t::stop_service() {
    controller->disable();
    client->disconnect();
}

bool ov_client_digitalstage_t::is_going_to_stop() const {
    return shouldQuit;
}

void ov_client_digitalstage_t::onReady(
        const DigitalStage::Api::Store *store) noexcept {
    try {
        auto localDevice = store->getLocalDevice();
        if (!localDevice)
            throw std::runtime_error("No local device found");

        auto tool = std::make_unique<sound_card_tools_t>();

        auto inputSoundDevices = tool->get_input_sound_cards();
        auto outputSoundDevices = tool->get_output_sound_cards();

        for (const auto &soundDevice: inputSoundDevices) {
            auto existingSoundCard =
                    store->getSoundCardByDeviceAndDriverAndTypeAndLabel(localDevice->_id, "jack", "input",
                                                                        soundDevice.name);
            if (existingSoundCard) {
                nlohmann::json payload;
                payload["_id"] = existingSoundCard->_id;
                payload["softwareLatency"] = soundDevice.software_latency;
                payload["isDefault"] = soundDevice.is_default;
                payload["online"] = true;
                if (existingSoundCard->channels.size() !=
                    soundDevice.num_channels) {
                    for (unsigned int i = 1; i <= soundDevice.num_channels; i++) {
                        if (i == 1) {
                            payload["channels"]["system:capture_" + std::to_string(i)] =
                                    true;
                        } else {
                            payload["channels"]["system:capture_" + std::to_string(i)] =
                                    false;
                        }
                    }
                }
                client->send(DigitalStage::Api::SendEvents::CHANGE_SOUND_CARD, payload);
            } else {
                // Send new sound card
                nlohmann::json payload;
                payload["label"] = soundDevice.name;
                payload["driver"] = "jack";
                payload["type"] = "input";
                payload["sampleRate"] = soundDevice.sample_rate;
                payload["sampleRates"] = soundDevice.sample_rates;
                payload["softwareLatency"] = soundDevice.software_latency;
                payload["isDefault"] = soundDevice.is_default;
                payload["numPeriods"] = 2;
                payload["online"] = true;
                for (unsigned int i = 1; i <= soundDevice.num_channels; i++) {
                    payload["channels"]["system:capture_" + std::to_string(i)] =
                            false;
                }
                if (!localDevice->inputSoundCardId &&
                    (soundDevice.is_default || inputSoundDevices.size() == 1)) {
                    // No sound card set yet, so use default (or the single found)
                    client->send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, payload,
                                 [&, localDevice](const nlohmann::json &result) {
                                     const std::string soundCardId = result[1];
                                     nlohmann::json update = {{"_id",              localDevice->_id},
                                                              {"inputSoundCardId", soundCardId}};
                                     client->send(
                                             DigitalStage::Api::SendEvents::CHANGE_DEVICE,
                                             update);
                                 });
                } else {
                    client->send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, payload);
                }
            }
        }
        //TODO: Reduce boilerplate here - could be merged into a short function
        for (const auto &soundDevice: outputSoundDevices) {
            auto existingSoundCard =
                    store->getSoundCardByDeviceAndDriverAndTypeAndLabel(localDevice->_id, "jack", "output",
                                                                        soundDevice.name);
            if (existingSoundCard) {
                nlohmann::json payload;
                payload["_id"] = existingSoundCard->_id;
                payload["softwareLatency"] = soundDevice.software_latency;
                payload["isDefault"] = soundDevice.is_default;
                payload["online"] = true;
                if (existingSoundCard->channels.size() !=
                    soundDevice.num_channels) {
                    for (unsigned int i = 1; i <= soundDevice.num_channels; i++) {
                        if (i == 1) {
                            payload["channels"]["system:playback_" + std::to_string(i)] =
                                    true;
                        } else {
                            payload["channels"]["system:playback_" + std::to_string(i)] =
                                    false;
                        }
                    }
                }
                client->send(DigitalStage::Api::SendEvents::CHANGE_SOUND_CARD, payload);
            } else {
                // Send new sound card
                nlohmann::json payload;
                payload["label"] = soundDevice.name;
                payload["driver"] = "jack";
                payload["type"] = "output";
                payload["sampleRate"] = soundDevice.sample_rate;
                payload["sampleRates"] = soundDevice.sample_rates;
                payload["softwareLatency"] = soundDevice.software_latency;
                payload["isDefault"] = soundDevice.is_default;
                payload["numPeriods"] = 2;
                payload["online"] = true;
                for (unsigned int i = 1; i <= soundDevice.num_channels; i++) {
                    payload["channels"]["system:playback_" + std::to_string(i)] =
                            false;
                }
                if (!localDevice->outputSoundCardId &&
                    (soundDevice.is_default || outputSoundDevices.size() == 1)) {
                    // No sound card set yet, so use default (or the single found)
                    client->send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, payload,
                                 [&, localDevice](const nlohmann::json &result) {
                                     const std::string soundCardId = result[1];
                                     nlohmann::json update = {{"_id",               localDevice->_id},
                                                              {"outputSoundCardId", soundCardId}};
                                     client->send(
                                             DigitalStage::Api::SendEvents::CHANGE_DEVICE,
                                             update);
                                 });
                } else {
                    client->send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, payload);
                }
            }
        }
    }
    catch (std::exception &exception) {
        std::cerr << "Internal error: " << exception.what() << std::endl;
    }
}