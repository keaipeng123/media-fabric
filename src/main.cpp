#include <iostream>
#include <string>

namespace {

const char* kDefaultConfigPath = "conf/gb28181-server.conf";

void printUsage(const char* programName)
{
    std::cout << "Usage: " << programName << " [-c config_path]" << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string configPath = kDefaultConfigPath;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "-h" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-c" || arg == "--config")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            configPath = argv[++i];
            continue;
        }

        std::cerr << "unknown argument: " << arg << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "gb28181-server bootstrap" << std::endl;
    std::cout << "config: " << configPath << std::endl;
    std::cout << "next: initialize GB28181Node and capability modules" << std::endl;

    return 0;
}
