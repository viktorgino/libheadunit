#include "command_server.h"
#include "json/json.hpp"

using json = nlohmann::json;

CommandServer::CommandServer(ICommandServerCallbacks &callbacks)
{
    server.get("/status", [&callbacks](WPP::Request& req, WPP::Response& resp)
    {
       resp.type = "application/json";
       json result;
       result["connected"] = callbacks.IsConnected();
       result["videoFocus"] = callbacks.HasVideoFocus();
       result["audioFocus"] = callbacks.HasAudioFocus();
       resp.body << std::setw(4) << result;
    });

    server.post("/takeVideoFocus", [&callbacks](WPP::Request& req, WPP::Response& resp)
    {
       resp.type = "application/json";
       callbacks.TakeVideoFocus();;
       json result;
       resp.body << std::setw(4) << result;
    });
}

bool CommandServer::Start()
{
    return server.start(9999);
}
