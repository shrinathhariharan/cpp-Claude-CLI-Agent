#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string execute_read_tool(const std::string& file_path)
{
    std::ifstream file{file_path};
    if (!file) return "Error: could not open file '" + file_path + "'";

    std::ostringstream contents{};
    contents << file.rdbuf();
    return contents.str();
}

std::string execute_write_tool(const std::string& file_path, const std::string& content)
{
    std::ofstream file{file_path};
    if (!file) {
        return "Error: could not open file '" + file_path + "' for writing";
    }

    file << content;

    if (!file) {
        return "Error: failed while writing to '" + file_path + "'";
    }

    return "Successfully wrote to '" + file_path + "'";
}

json send_request(const std::string& base_url, const std::string& api_key, const json& messages, const json& tools)
{
    json request_body = {
        {"model", "anthropic/claude-haiku-4.5"},
        {"messages", messages},
        {"tools", tools}
    };

    cpr::Response response = cpr::Post(
        cpr::Url{base_url + "/chat/completions"},
        cpr::Header{
            {"Authorization", "Bearer " + api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{request_body.dump()}
    );

    if (response.status_code != 200)
    {
        std::cerr << "HTTP error: " << response.status_code << std::endl;
        std::exit(1);
    }

    return json::parse(response.text);
}

std::string execute_bash_tool(const std::string& command)
{
    std::string full_command = command + " 2>&1";

    std::array<char, 4096> buffer;
    std::string output;

    FILE* pipe = popen(full_command.c_str(), "r");
    if (!pipe) {
        return "Error: failed to execute command";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }

    int exit_code = pclose(pipe);

    if (exit_code != 0) {
        output += "\n[Command exited with code " + std::to_string(exit_code) + "]";
    } else if (output.empty()) {
        // Some commands (rm, mv, mkdir, etc.) produce no output on success.
        // Return something non-empty so the model gets clear confirmation
        // rather than an ambiguous blank string.
        output = "[Command completed successfully with no output]";
    }

    return output;
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

    json tools = json::array({
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
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "Write"},
                {"description", "Write content to a file, overwriting it if it already exists"},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"file_path", {
                            {"type", "string"},
                            {"description", "The path to the file to write"}
                        }},
                        {"content", {
                            {"type", "string"},
                            {"description", "The content to write to the file"}
                        }}
                    }},
                    {"required", json::array({"file_path", "content"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "Bash"},
                {"description", "Execute a shell command and return its output"},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"command", {
                            {"type", "string"},
                            {"description", "The shell command to execute"}
                        }}
                    }},
                    {"required", json::array({"command"})}
                }}
            }}
        }
    });

    json write_tool_spec = {
        {"type", "function"},
        {"function", {
            {"name", "Write"},
            {"description", "Write content to a file, overwriting it if it already exists"},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"file_path", {
                        {"type", "string"},
                        {"description", "The path to the file to write"}
                    }},
                    {"content", {
                        {"type", "string"},
                        {"description", "The content to write to the file"}
                    }}
                }},
                {"required", json::array({"file_path", "content"})}
            }}
        }}
    };

    json bash_tool_spec = {
        {"type", "function"},
        {"function", {
            {"name", "Bash"},
            {"description", "Execute a shell command and return its output"},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"command", {
                        {"type", "string"},
                        {"description", "The shell command to execute"}
                    }}
                }},
                {"required", json::array({"command"})}
            }}
        }}
    };

    json messages = json::array({
        {{"role", "user"}, {"content", prompt}}
    });

    while (true)
    {
        json result = send_request(base_url, api_key, messages, tools);

        if (!result.contains("choices") || result["choices"].empty())
        {
            std::cerr << "No choices in response" << std::endl;
            return 1;
        }

        json message = result["choices"][0]["message"];

        messages.push_back(message);

        bool has_tool_calls = message.contains("tool_calls") && !message["tool_calls"].is_null();

        if (!has_tool_calls) {
            std::cout << message["content"].get<std::string>();
            return 0;
        }

        //execute all requested tools and give results
        for (const auto& tool_call : message["tool_calls"]) {
            std::string tool_call_id = tool_call["id"].get<std::string>();
            std::string tool_name = tool_call["function"]["name"].get<std::string>();

            json arguments = json::parse(
                tool_call["function"]["arguments"].get<std::string>()
            );

            std::string tool_result;

            if (tool_name == "Read") {
                std::string file_path = arguments["file_path"].get<std::string>();
                tool_result = execute_read_tool(file_path);
            }
            else if (tool_name == "Write") {
                std::string file_path = arguments["file_path"].get<std::string>();
                std::string content = arguments["content"].get<std::string>();
                tool_result = execute_write_tool(file_path, content);
            }
            else if (tool_name == "Bash") {
                std::string command = arguments["command"].get<std::string>();
                tool_result = execute_bash_tool(command);
            }
            else {
                tool_result = "Error: unknown tool '" + tool_name + "'";
            }

            messages.push_back({
                {"role", "tool"},
                {"tool_call_id", tool_call_id},
                {"content", tool_result}
            });
        }
    }

    

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

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    //std::cerr << "Logs from your program will appear here!" << std::endl;

    //std::cout << result["choices"][0]["message"]["content"].get<std::string>();

    return 0;
}
