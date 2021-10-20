#include <gtest/gtest.h>

#include "ov_ds_config.h"

#include <filesystem>
#include <fstream>

#include <xercesc/util/PlatformUtils.hpp>

TEST(digital_stage_config, readExistingWellFormatted_valuesAsExpected)
{
    xercesc::XMLPlatformUtils::Initialize();

    using namespace std::filesystem;
    path configPath = temp_directory_path() / "dsconfig_sample_xml.xml";
    if(exists(configPath))
        remove_all(configPath);

    std::string wellFormatted(
           "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n"\
           "<config>\n"\
           "    <login>\n"\
           "        <login>some.email@some-domain.com</login>\n"\
           "        <password>secret</password>\n"\
           "    </login>\n"\
           "    <server>\n"\
           "        <auth-url>https://auth.dstage.org</auth-url>\n"\
           "        <api-url>wss://api.dstage.org</api-url>\n"\
           "    </server>\n"\
           "</config>\n");

    // dump to file
    std::ofstream testFile(configPath.string());
    testFile << wellFormatted;
    testFile.close();

    // act
    try {
        DsConfig config(configPath.string());
        EXPECT_EQ("some.email@some-domain.com", config.login());
        EXPECT_EQ("secret", config.pass());
        EXPECT_EQ("https://auth.dstage.org", config.auth_url());
        EXPECT_EQ("wss://api.dstage.org", config.api_url());

    } catch(std::runtime_error const & e) {
        // exception - test fail
        EXPECT_TRUE(false) << "Config parser threw exception: " << e.what();
        FAIL();
    }

    xercesc::XMLPlatformUtils::Terminate();

    if(exists(configPath))
        remove_all(configPath);
}

TEST(digital_stage_config, readExistingErrors_throwsException)
{
    xercesc::XMLPlatformUtils::Initialize();

    using namespace std::filesystem;
    path configPath = temp_directory_path() / "dsconfig_sample_xml_wrong.xml";
    if(exists(configPath))
        remove_all(configPath);

    std::string wellFormatted(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n"\
           "<malformatted>\n"\
           "    <login>\n"\
           "        <login>some.email@some-domain.com</login>\n"\
           "        <password>secret</password>\n"\
           "    </login>\n"\
           "    <server>\n"\
           "        <auth-url>https://auth.dstage.org</auth-url>\n"\
           "        <api-url>wss://api.dstage.org</api-url>\n"\
           "    </server>\n"\
           "</config>\n");

    // dump to file
    std::ofstream testFile(configPath.string());
    testFile << wellFormatted;
    testFile.close();

    // act
    EXPECT_THROW({DsConfig config(configPath.string());},std::runtime_error);

    xercesc::XMLPlatformUtils::Terminate();

    if(exists(configPath))
        remove_all(configPath);
}