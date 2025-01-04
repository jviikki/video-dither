#include <optional>
#include <string>

struct CommandLineArgs {
    std::optional<int> height;
    std::optional<int> width;
    std::optional<int> colors;
    bool two_bit = false;
    std::string input_file;
    std::string output_file;
};

CommandLineArgs parseArguments(int argc, char* argv[]);