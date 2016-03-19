#include <iostream>
#include <stdlib.h>
#include <fstream>
#include "Simple-Web-Server/server_http.hpp"

using namespace std;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

int main()
{
    HttpServer server(8087, 1);

    server.default_resource["POST"] = server.default_resource["GET"] = [](HttpServer::Response& response, shared_ptr<HttpServer::Request> request)
    {

        auto content=request->content.string();

        cout << content << endl;

        system("service minecraft command 'tellraw @p {\"text\":\"<mattermost:username> Hello World!\"}'");

        response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;

        // response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };

    thread server_thread([&server]()
    {
        server.start();
    });

    server_thread.join();

    return 0;
}
