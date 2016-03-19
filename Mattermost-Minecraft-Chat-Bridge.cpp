#include <iostream>
#include <algorithm>
#include <stdlib.h>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/regex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Simple-Web-Server/server_http.hpp"

using namespace std;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

unsigned char from_hex(unsigned char ch)
{
  if (ch <= '9' && ch >= '0')
  ch -= '0';
  else if (ch <= 'f' && ch >= 'a')
  ch -= 'a' - 10;
  else if (ch <= 'F' && ch >= 'A')
  ch -= 'A' - 10;
  else
  ch = 0;
  return ch;
}

string urldecode(const string& str)
{
  string result;
  string::size_type i;
  for (i = 0; i < str.size(); ++i)
  {
    if (str[i] == '+')
    {
      result += ' ';
    }
    else if (str[i] == '%' && str.size() > i+2)
    {
      const unsigned char ch1 = from_hex(str[i+1]);
      const unsigned char ch2 = from_hex(str[i+2]);
      const unsigned char ch = (ch1 << 4) | ch2;
      result += ch;
      i += 2;
    }
    else
    {
      result += str[i];
    }
  }
  return result;
}

map<string, string> parse_url(std::string word)
{
  string::size_type pos;
  map<string, string> queries;

  for (pos = 0; pos < word.size(); ++pos)
  {
    if (word[pos] == '&')
    word[pos] = ' ';
  }

  istringstream sin(word);
  sin >> word;
  while (sin)
  {
    pos = word.find_first_of("=");
    if (pos != std::string::npos)
    {
      string key = urldecode(word.substr(0,pos));
      string value = urldecode(word.substr(pos+1));

      queries[key] = value;
    }
    sin >> word;
  }
  return queries;
}

int main()
{
  boost::property_tree::ptree config;
  int port, threads;
  string outToken, webhook, mc_log, service, m2mm, mm2m;
  try
  {
    boost::property_tree::read_json("config.json", config);
    port    = config.get<int>("port");
    threads = config.get<int>("threads");
    outToken= config.get<string>("token");
    webhook = config.get<string>("webhook");
    mc_log  = config.get<string>("minecraft_log");
    service = config.get<string>("minecraft_service");
    m2mm    = config.get<string>("minecraft_to_mattermost_prefix");
    mm2m    = config.get<string>("mattermost_to_minecraft_prefix");
  }
  catch(exception& e)
  {
    cout << "Error parsing config.json: " << e.what() << endl;
    return 1;
  }

  HttpServer server(port, threads);
  server.default_resource["POST"] = server.default_resource["GET"] = [outToken,service,mm2m](HttpServer::Response& response, shared_ptr<HttpServer::Request> request)
  {
    try
    {
      auto content = request->content.string();
      auto params  = parse_url(content);

      auto team_id      = params["team_id"];
      auto team_domain  = params["team_domain"];

      auto channel_id   = params["channel_id"];
      auto channel_name = params["channel_name"];

      auto user_id      = params["user_id"];
      auto user_name    = params["user_name"];

      auto trigger_word = params["trigger_word"];
      auto text         = params["text"];

      auto timestamp    = params["timestamp"];
      auto token        = params["token"];

      boost::regex re1("\"");
      boost::regex re2("'");
      text = boost::regex_replace(text, re1, "\\\\u0022");
      text = boost::regex_replace(text, re2, "\\\\u0027");

      auto command = "service "+service+" command 'tellraw @a {\"text\":\"<"+mm2m+":"+user_name+"> "+text+"\"}'";
      system(command.c_str());
      cout << command << endl;
      response << "HTTP/1.1 200 OK\r\nContent-Length: 0 \r\n\r\n";
    }
    catch(exception& e)
    {
      response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
    }
  };

  thread server_thread([&server](){ server.start(); });
  server_thread.join();
  return 0;
}
