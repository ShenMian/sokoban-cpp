// Copyright 2023 ShenMian
// License(Apache-2.0)

#include "sokoban.hpp"

#include "crc32.hpp"
#include "level.hpp"

int main(int argc, char* argv[])
{
	std::cout << R"(
   ____     __        __           
  / __/__  / /_____  / /  ___ ____ 
 _\ \/ _ \/  '_/ _ \/ _ \/ _ `/ _ \
/___/\___/_/\_\\___/_.__/\_,_/_//_/

)";
	/*
	std::cout << R"(
          1. Default
		  2. Open from file
          3. Open from clipboard

)";
*/

	try
	{
		Sokoban sokoban;
		sokoban.run(argc, argv);
	}
	catch(std::runtime_error& e)
	{
		std::cerr << "ERROR: " << e.what() << "\n";
		std::cerr << "Press enter to exit...\n";
		std::string line;
		std::getline(std::cin, line);
		return 1;
	}

	return 0;
}