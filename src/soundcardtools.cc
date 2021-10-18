/*
 * This file is part of the ovbox software tool, see <http://orlandoviols.com/>.
 *
 * Copyright (c) 2021 Giso Grimm, delude88, Tobias Hegemann
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
 * Version 3 along with ovbox. If not, see <http://www.gnu.org/licenses/>.
 */

#include "soundcardtools.h"

#include <iostream>

#ifndef __APPLE__
#include <alsa/asoundlib.h>
#endif

#include "common.h"

std::vector<snddevname_t> list_sound_devices() {
    std::vector<snddevname_t> retv;
#ifndef __APPLE__
    char** hints;
    int err;
    char** n;
    char* name;
    char* desc;

    /* Enumerate sound devices */
    err = snd_device_name_hint(-1, "pcm", (void***)&hints);
    if(err != 0) {
      std::cerr << "Warning: unable to get name hints (list_sound_devices)"
                << std::endl;
      return retv;
    }
    n = hints;
    while(*n != NULL) {
      name = snd_device_name_get_hint(*n, "NAME");
      desc = snd_device_name_get_hint(*n, "DESC");
      if(strncmp("hw:", name, 3) == 0) {
        snddevname_t dname;
        dname.dev = name;
        dname.desc = desc;
        if(dname.desc.find("\n") != std::string::npos)
          dname.desc.erase(dname.desc.find("\n"));
        retv.push_back(dname);
      }
      if(name && strcmp("null", name))
        free(name);
      if(desc && strcmp("null", desc))
        free(desc);
      n++;
    }
    // Free hint buffer too
    snd_device_name_free_hint((void**)hints);
    if(retv.empty()) {
      int card(-1);
      while(snd_card_next(&card) == 0) {
        if(card == -1)
          break;
        snddevname_t dname;
        char* card_name(NULL);
        if(0 == snd_card_get_name(card, &card_name)) {
          dname.dev = "hw:" + std::to_string(card);
          dname.desc = card_name;
          free(card_name);
          retv.push_back(dname);
        }
      }
    }
#else
    auto *soundCardUtil = new sound_card_tools_t();
    auto input = soundCardUtil->get_input_sound_cards();
    auto output = soundCardUtil->get_output_sound_cards();
    retv.reserve(input.size() + output.size());
    for (auto &soundDevice: input) {
        retv.push_back({soundDevice.id, soundDevice.name});
    }
    for (auto &soundDevice: output) {
        retv.push_back({soundDevice.id, soundDevice.name});
    }
#endif
    return retv;
}

std::string url2localfilename(const std::string &url) {
    if (url.empty())
        return url;
    std::string extension(url);
    size_t pos = extension.find("?");
    if (pos != std::string::npos)
        extension.erase(pos, extension.size() - pos);
    pos = extension.find(".");
    if (pos == std::string::npos)
        extension = "";
    else {
        while ((pos = extension.find(".")) != std::string::npos)
            extension.erase(0, pos + 1);
        if (!extension.empty())
            extension = "." + extension;
    }
    return std::to_string(std::hash<std::string>{}(url)) + extension;
}

#ifndef LINUX

sound_card_tools_t::sound_card_tools_t() {
    this->soundio = soundio_create();
    soundio_connect(this->soundio); // use default driver (not always jack)
}

sound_card_tools_t::~sound_card_tools_t() {
    soundio_disconnect(this->soundio);
    delete this->soundio;
}


sound_card_t sound_card_tools_t::convert(struct SoundIoDevice *sound_device, bool is_default = false) {
    sound_card_t soundcard;
    soundcard.id = std::string(sound_device->id);
    soundcard.name = std::string(sound_device->name);
    soundcard.num_channels = sound_device->current_layout.channel_count;
    soundcard.sample_rate = sound_device->sample_rate_current;

    for (int j = 0; j < sound_device->sample_rate_count; j += 1) {
        struct SoundIoSampleRateRange *range =
                &sound_device->sample_rates[j];
        soundcard.sample_rates.push_back(range->min);
    }
    soundcard.is_default = is_default;
    soundcard.software_latency =
            sound_device->software_latency_current;
    return soundcard;
}

std::vector<struct SoundIoDevice *> sound_card_tools_t::get_raw_input_sound_devices() {
    soundio_flush_events(this->soundio); // Flush the sound devices
    int input_count = soundio_input_device_count(this->soundio);
    // int default_input = soundio_default_input_device_index(this->soundio);
    std::vector<struct SoundIoDevice *> input_sound_devices;
    for (int i = 0; i < input_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_input_device(this->soundio, i);
        input_sound_devices.push_back(device);
        soundio_device_unref(device);
    }
    return input_sound_devices;
}


std::vector<struct SoundIoDevice *>
sound_card_tools_t::get_raw_output_sound_devices() {
    soundio_flush_events(this->soundio); // Flush the sound devices
    int output_count = soundio_output_device_count(this->soundio);
    std::vector<struct SoundIoDevice *> output_sound_devices;
    for (int i = 0; i < output_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_output_device(this->soundio, i);
        output_sound_devices.push_back(device);
        soundio_device_unref(device);
    }
    return output_sound_devices;
}

struct SoundIo *sound_card_tools_t::get_sound_io() {
    return this->soundio;
}

std::vector<sound_card_t> sound_card_tools_t::get_input_sound_cards() {

    if(nullptr == this->soundio->userdata)
        return std::vector<sound_card_t>();

    int default_input = soundio_default_input_device_index(this->soundio);
    const std::vector<struct SoundIoDevice *> input_sound_devices =
            this->get_raw_input_sound_devices();

    std::vector<sound_card_t> soundcards;
    for (size_t i = 0; i < input_sound_devices.size(); i++) {
        SoundIoDevice *input_sound_device = input_sound_devices[i];
        bool is_default = (size_t) default_input == i;
        soundcards.emplace_back(std::move(this->convert(input_sound_device, is_default)));
    }
    return soundcards;
}


std::vector<sound_card_t> sound_card_tools_t::get_output_sound_cards() {
    if(nullptr == this->soundio->userdata)
        return std::vector<sound_card_t>();

    int default_output = soundio_default_output_device_index(this->soundio);
    const std::vector<struct SoundIoDevice *> output_sound_devices =
            this->get_raw_output_sound_devices();
    std::vector<sound_card_t> soundcards;
    for (size_t i = 0; i < output_sound_devices.size(); i++) {
        SoundIoDevice *output_sound_device = output_sound_devices[i];
        bool is_default = (size_t) default_output == i;
        soundcards.emplace_back(std::move(this->convert(output_sound_device, is_default)));
    }
    return soundcards;
}

#endif

/*
 * Local Variables:
 * compile-command: "make -C .."
 * End:
 */
