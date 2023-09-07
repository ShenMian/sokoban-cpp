// Copyright 2023 ShenMian
// License(Apache-2.0)

#include "sokoban.hpp"
#include <filesystem>

int main(int argc, char* argv[])
{
	std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());

	try
	{
		Sokoban sokoban;
		sokoban.run(argc, argv);
	}
	catch(const std::runtime_error& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
		std::cerr << "Press enter to exit...\n";
		std::string line;
		std::getline(std::cin, line);
		std::getline(std::cin, line);
		return 1;
	}
	catch(...)
	{
		std::cerr << "Unknown exception\n";
		std::cerr << "Press enter to exit...\n";
		std::string line;
		std::getline(std::cin, line);
		std::getline(std::cin, line);
		return 1;
	}

	return 0;
}
