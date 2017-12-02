#include "../client/utils.h"

#include "prt/AttributeMap.h"

#include "boost/filesystem/path.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"


TEST_CASE("create file URI from path", "[utils]" ) {
    REQUIRE(toFileURI(boost::filesystem::path("/tmp/foo.txt")) == L"file:/tmp/foo.txt");
}

TEST_CASE("percent-encode a UTF-8 string", "[utils]") {
    REQUIRE(percentEncode("with space") == L"with%20space");
}

TEST_CASE("get XML representation of a PRT object") {
    AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create()); // TODO: AttributeMapBuilder::operator<< is broken, report
	amb->setString(L"foo", L"bar");
    AttributeMapUPtr am(amb->createAttributeMap());
    std::string xml = objectToXML(am.get());
    REQUIRE(xml == "<attributable>\n\t<attribute key=\"foo\" value=\"bar\" type=\"str\"/>\n</attributable>");
}