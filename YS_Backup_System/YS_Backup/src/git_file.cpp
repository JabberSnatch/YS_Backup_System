#include "git_file.hpp"

#include <fstream>
#include <sstream>

namespace ys {
namespace git {
namespace file {


void
file_resolve_conflicts(const std::string& path, ResolutionStyle style)
{
	// NOTE: Could use some safety check.
	std::fstream		file(path, std::ios_base::in | std::ios_base::app);
	std::string			contents((std::istreambuf_iterator<char>(file)),
								 std::istreambuf_iterator<char>());
	std::stringstream	buffer(contents);
	file.close();

	// NOTE: Ours : <<<<<<< HEAD -> =======
	//		 Theirs : ======= -> >>>>>>> [commit id]
	std::string			line;
	ResolutionStyle		current_block = kStyleDefault;
	std::stringstream	result;
	while (!std::getline(buffer, line).eof())
	{
		std::string		beginning = line.substr(0, 7);
		bool			state_changed = false;

		if (beginning == "<<<<<<<")
		{
			current_block = kStyleOurs;
			state_changed = true;
		}
		if (beginning == "=======")
		{
			current_block = kStyleTheirs;
			state_changed = true;
		}
		if (beginning == ">>>>>>>")
		{
			current_block = kStyleDefault;
			state_changed = true;
		}
		
		if (!state_changed && 
			(current_block == kStyleDefault ||
			current_block == style))
			result << line << std::endl;
	}

	file.open(path, std::ios_base::out);
	file << result.str();
	file.close();
}


} // namespace file
} // namespace git
} // namespace ys
