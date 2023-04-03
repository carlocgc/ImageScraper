#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>


int main(int argc, char* argv[])
{

    // Initialize curlpp
    curlpp::Cleanup cleanup;
    curlpp::Easy request;

    // Set the URL to request
    request.setOpt(new curlpp::options::Url("http://google.com"));

    // Send
    std::ostringstream response;
    request.setOpt(new curlpp::options::WriteStream(&response));
    request.perform();

    // Print response
    std::cout << response.str() << std::endl;

    return 0;
}