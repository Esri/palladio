#include "../client/utils.h"

#include "boost/filesystem/path.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"


TEST_CASE("create file URI from path", "[utils]" ) {
    REQUIRE(p4h::utils::toFileURI(boost::filesystem::path("/tmp/foo.txt")) == L"file:/tmp/foo.txt");
}

TEST_CASE("percent-encode a UTF-8 string", "[utils]") {
    REQUIRE(p4h::utils::percentEncode("with space") == L"with%20space");
}