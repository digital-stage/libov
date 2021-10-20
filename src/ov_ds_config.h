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
#ifndef _OV_DS_CONFIG_H_
#define _OV_DS_CONFIG_H_

#include <string_view>
#include <string>

class DsConfig {
public:
    DsConfig(std::string_view configPath);
    ~DsConfig() = default;

    DsConfig(const DsConfig &) = delete;
    DsConfig & operator= (const DsConfig &) = delete;

    std::string_view login() const;
    std::string_view pass() const;
    std::string_view api_url() const;
    std::string_view auth_url() const;

private:
    void parseConfig(std::string_view configPath);

private:
    std::string _login;
    std::string _pass;
    std::string _api_url;
    std::string _auth_url;
};

#endif