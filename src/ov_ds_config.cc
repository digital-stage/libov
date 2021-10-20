/*
*
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
#include "ov_ds_config.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/TransService.hpp>

#include <filesystem>

DsConfig::DsConfig(std::string_view configPath)
{
    parseConfig(std::move(configPath));
}

std::string_view DsConfig::login() const
{
    return _login;
}

std::string_view DsConfig::pass() const
{
    return _pass;
}

std::string_view DsConfig::api_url() const
{
    return _api_url;
}

std::string_view DsConfig::auth_url() const
{
    return _auth_url;
}

void DsConfig::parseConfig(std::string_view configPath) {
    using namespace xercesc;

    const auto LOGIN = XMLString::transcode("/config/login/login");
    const auto PASS = XMLString::transcode("/config/login/password");
    const auto AUTH_URL = XMLString::transcode("/config/server/auth-url");
    const auto API_URL = XMLString::transcode("/config/server/api-url");

    // check for path
    std::filesystem::path path(configPath);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(std::string("Could not find digital stage config file ") + configPath.data());
    }

    // init should be done by static raii object in libtascar
    XercesDOMParser parser;
    parser.setValidationScheme(XercesDOMParser::Val_Never);
    parser.setDoNamespaces(false);
    parser.setDoSchema(false);
    parser.setLoadExternalDTD(false);

    parser.parse(configPath.data());

    DOMDocument *doc = parser.getDocument();
    // get the root element
    DOMElement* root = doc->getDocumentElement();
    auto const resultType = DOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE;

    // evaluate the xpath
    auto resultLogin = doc->evaluate( LOGIN, root, nullptr, resultType, nullptr);
    auto resultPass = doc->evaluate( PASS, root, nullptr, resultType, nullptr);
    auto resultAuth = doc->evaluate( AUTH_URL, root, nullptr, resultType, nullptr);
    auto resultApi = doc->evaluate( API_URL, root, nullptr, resultType, nullptr);

    if(!resultLogin->getNodeValue()  || !resultPass->getNodeValue()  ||
       !resultAuth->getNodeValue() || !resultApi->getNodeValue() ) {
        throw std::runtime_error( std::string("Invalid configuration: " ) + configPath.data());
    }

    _login = XMLString::transcode(resultLogin->getNodeValue()->getFirstChild()->getNodeValue());
    _pass = XMLString::transcode(resultPass->getNodeValue()->getFirstChild()->getNodeValue());
    _api_url = XMLString::transcode(resultApi->getNodeValue()->getFirstChild()->getNodeValue());
    _auth_url = XMLString::transcode(resultAuth->getNodeValue()->getFirstChild()->getNodeValue());
}

