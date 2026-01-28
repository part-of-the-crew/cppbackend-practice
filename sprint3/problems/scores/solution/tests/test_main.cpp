#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

int main(int argc, char* argv[]) {
    // You can add global setup/teardown here if needed in the future
    return Catch::Session().run(argc, argv);
}