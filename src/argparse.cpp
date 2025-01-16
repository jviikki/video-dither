#include <iostream>
#include <vector>
#include <stdexcept>
#include <unordered_map>

#include "argparse.hpp"

CommandLineArgs parseArguments(int argc, char* argv[]) {
    CommandLineArgs args;

    if (argc < 3) {
        throw std::invalid_argument("Usage: ./video-dither [-h height | --height=height] [-w width | --width=width] [--two-bit | --colors=number_of_colors] input_file output_file");
    }

    std::vector<std::string> arguments(argv + 1, argv + argc);

    for (size_t i = 0; i < arguments.size(); ++i) {
        const std::string& arg = arguments[i];

        if (arg == "-h" || arg.find("--height=") == 0) {
            if (arg == "-h") {
                if (i + 1 >= arguments.size()) {
                    throw std::invalid_argument("Missing value for -h");
                }
                args.height = std::stoi(arguments[++i]);
            } else {
                args.height = std::stoi(arg.substr(9));
            }
        } else if (arg == "-w" || arg.find("--width=") == 0) {
            if (arg == "-w") {
                if (i + 1 >= arguments.size()) {
                    throw std::invalid_argument("Missing value for -w");
                }
                args.width = std::stoi(arguments[++i]);
            } else {
                args.width = std::stoi(arg.substr(8));
            }
        } else if (arg == "--1-bit") {
            args.one_bit = true;
        } else if (arg.find("--colors=") == 0) {
            args.colors = std::stoi(arg.substr(9));
        } else if (i == arguments.size() - 2) {
            args.input_file = arg;
        } else if (i == arguments.size() - 1) {
            args.output_file = arg;
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    if (args.input_file.empty() || args.output_file.empty()) {
        throw std::invalid_argument("Missing input or output file");
    }

    if (args.one_bit && args.colors.has_value()) {
        throw std::invalid_argument("Cannot specify both --two-bit and --colors");
    }

    return args;
}
