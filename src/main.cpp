#include <iostream>
#include <thread>

#include "argparse.hpp"
#include "concurrency.hpp"

int main(int argc, char* argv[]) {
    try {
        CommandLineArgs args = parseArguments(argc, argv);
        processFramesConcurrently(args);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
