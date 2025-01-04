#include <iostream>
#include <thread>

#include "argparse.hpp"
#include "concurrency.hpp"

int main(int argc, char* argv[]) {
    try {
        CommandLineArgs args = parseArguments(argc, argv);

        // Print parsed arguments
        std::cout << "Height: " << (args.height ? std::to_string(*args.height) : "Not provided") << "\n";
        std::cout << "Width: " << (args.width ? std::to_string(*args.width) : "Not provided") << "\n";
        std::cout << "Two-bit mode: " << (args.two_bit ? "Enabled" : "Disabled") << "\n";
        std::cout << "Colors: " << (args.colors ? std::to_string(*args.colors) : "Not provided") << "\n";
        std::cout << "Input file: " << args.input_file << "\n";
        std::cout << "Output file: " << args.output_file << "\n";

        processFramesConcurrently(args.input_file, args.output_file);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
