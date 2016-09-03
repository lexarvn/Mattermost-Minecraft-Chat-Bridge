#include <iostream>
#include <algorithm>
#include <future>
#include <stdlib.h>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/regex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <curl/curl.h>
#include "Simple-Web-Server/server_http.hpp"
#include "inotify-cxx/inotify-cxx.cpp"

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

string sendMsgToMinecraft(string request, string outToken, string service, string mm2m)
{
  auto params  = parse_url(request);

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

  return "HTTP/1.1 200 OK\r\nContent-Length: 0 \r\n\r\n";
}

void sendMsgToMattermost(string logMsg, string webhook, string m2mm, string iconUrl)
{
  boost::smatch what;
  boost::regex expression("^\\[[0-9]{2}:[0-9]{2}:[0-9]{2}\\] \\[Server thread\\/INFO\\]: <([^>]+)> (.+)$");
  if(boost::regex_match(logMsg, what, expression))
  {
    string username = what[1];
    string message = what[2];

    boost::regex re("\"");
    message = boost::regex_replace(message, re, "\\\\u0022");

    string payload = "payload={\"username\":\""+m2mm+":"+username+"\",\"icon_url\": \""+iconUrl+"\",\"text\":\""+message+"\"}";

    CURL *curl;
    CURLcode res;

    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, webhook.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    cout << payload << endl;
  }
}

int main()
{
  boost::property_tree::ptree config;
  int port, threads;
  string outToken, webhook, mc_log, service, m2mm, mm2m, iconUrl;
  try
  {
    boost::property_tree::read_json("config.json", config);

    port    = config.get<int>("port");
    threads = config.get<int>("threads");

    outToken= config.get<string>("token");
    service = config.get<string>("minecraft_service");
    mm2m    = config.get<string>("mattermost_to_minecraft_prefix");

    webhook = config.get<string>("webhook");
    mc_log  = config.get<string>("minecraft_log");
    m2mm    = config.get<string>("minecraft_to_mattermost_prefix");
    iconUrl = config.get<string>("icon_url");
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
      response << sendMsgToMinecraft(request->content.string(),outToken,service,mm2m);
    }
    catch(exception& e)
    {
      response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
    }
  };

  auto future = async(launch::async, [&server](){ server.start(); });

  condition_variable cv;
  atomic<bool> readLog(false);
  atomic<bool> running(true);
  Inotify notify;
  InotifyWatch watch(mc_log, IN_MODIFY);
  notify.Add(watch);

  thread worker([&cv,&running,&readLog,mc_log,webhook,m2mm,iconUrl]()
  {
    mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    ifstream initLog(mc_log);
    string line;
    while (initLog >> ws && getline(initLog, line)); //get the last line
    initLog.seekg(0, initLog.end);
    auto oldPosition = initLog.tellg();
    initLog.close();

    while(running)
    {
        cv.wait(lck,[&readLog](){return readLog.load();});
        readLog.store(false);

        ifstream logFile(mc_log);
        logFile.seekg(0, logFile.end);
        auto newPosition = logFile.tellg();
        logFile.seekg(0, logFile.beg);
        if(oldPosition <= newPosition) logFile.seekg(oldPosition);

        do {
          getline(logFile, line);
          logFile >> ws;
          sendMsgToMattermost(line, webhook, m2mm, iconUrl);
        } while(logFile.good());

        oldPosition = newPosition;
        logFile.close();
    }
  });

  for (auto status = future.wait_for(chrono::milliseconds(0)); status != future_status::ready; status = future.wait_for(chrono::milliseconds(0))) {
    notify.WaitForEvents();
    readLog.store(true);
    cv.notify_one();
  }

  return 0;
}
