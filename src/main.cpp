#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string execute_read_tool(const json& arguments)
{
    std::string file_path = arguments["file_path"].get<std::string>();

    std::ifstream file{file_path};
    if (!file) return "Error: could not open file '" + file_path + '\'';

    std::ostringstream contents{};
    contents << file.rdbuf();
    return contents.str();
}

int main(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[1]) != "-p") {
        std::cerr << "Expected first argument to be '-p'" << std::endl;
        return 1;
    }

    std::string prompt = argv[2];

    if (prompt.empty()) {
        std::cerr << "Prompt must not be empty" << std::endl;
        return 1;
    }

    const char* api_key_env = std::getenv("OPENROUTER_API_KEY");
    const char* base_url_env = std::getenv("OPENROUTER_BASE_URL");

    std::string api_key = api_key_env ? api_key_env : "";
    std::string base_url = base_url_env ? base_url_env : "https://openrouter.ai/api/v1";

    if (api_key.empty()) {
        std::cerr << "OPENROUTER_API_KEY is not set" << std::endl;
        return 1;
    }

    json messages = json::array({
        {{"role", "user"}, {"content", prompt}}
    });

    

    json request_body = {
        {"model", "anthropic/claude-haiku-4.5"},
        {"messages", json::array({
            {{"role", "user"}, {"content", prompt}}
        })},
        {"tools", json::array({
            {
                {"type", "function"},
                {"function", {
                    {"name", "Read"},
                    {"description", "Read and return the contents of a file"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"file_path", {
                                {"type", "string"},
                                {"description", "The path to the file to read"}
                            }}
                        }},
                        {"required", json::array({"file_path"})}
                    }}
                }}
            }
        })}
    };

    cpr::Response response = cpr::Post(
        cpr::Url{base_url + "/chat/completions"},
        cpr::Header{
            {"Authorization", "Bearer " + api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{request_body.dump()}
    );

    if (response.status_code != 200) {
        std::cerr << "HTTP error: " << response.status_code << std::endl;
        return 1;
    }

    json result = json::parse(response.text);

    if (!result.contains("choices") || result["choices"].empty()) {
        std::cerr << "No choices in response" << std::endl;
        return 1;
    }

    json message = result["choices"][0]["message"];

    if (message.contains("tool_calls") && !message["tool_calls"].is_null())
    {
        json tool_call = message["tool_calls"][0];

        std::string tool_name = tool_call["function"]["name"].get<std::string>();

        json arguments = json::parse(
            tool_call["function"]["arguments"].get<std::string>()
        );

        if (tool_name == "Read")
        {
            std::string file_path = arguments["file_path"].get<std::string>();
            std::string file_contents = execute_read_tool(file_path);

            std::cout << file_contents;
        }
        else
        {
            std::cerr << "Unkown tool: " << tool_name << std::endl;
            return 1;
        }
    }
    else {
        std::cout << message["content"].get<std::string>();
    }

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cerr << "Logs from your program will appear here!" << std::endl;

    std::cout << result["choices"][0]["message"]["content"].get<std::string>();

    return 0;
}
