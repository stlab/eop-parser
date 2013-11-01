#include <iostream>
#include <fstream>
#include <adobe/array.hpp>
#include "exp_parser.hpp"

namespace {

std::string get_line(adobe::name_t n, std::streampos p)
{
    std::string result;
    std::ifstream s(n.c_str());
    p -= 1; // REVISIT (sparent) : This shouldn't be necessary!
    s.seekg(p);
    std::getline(s, result);
#if 0
    std::getline(s, result);
    std::cout << result << std::endl;
    std::getline(s, result);
    std::cout << result << std::endl;
#endif
    return result;
}

} // namespace

int main (int argc, char * const argv[]) {
    const char* file((argc > 1) ? argv[1] : "/Users/sparent/Development/projects/eop_code/eop.hpp");

    try {
        std::ifstream stream(file);

        eop::expression_parser parser(stream,
            adobe::line_position_t(adobe::name_t(file),
                adobe::line_position_t::getline_proc_t(new adobe::line_position_t::getline_proc_impl_t(&get_line))));

        parser.parse();
        std::cout << "Success!" << std::endl;

    } catch (const adobe::stream_error_t& error) {

        std::cerr << format_stream_error(error);

    } catch (const std::exception& error) {

        std::cerr << error.what();
    }

    return 0;
}
