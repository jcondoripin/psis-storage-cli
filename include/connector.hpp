#pragma once

#include "client.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

using Record = std::unordered_map<std::string, std::string>;

struct DBResponse
{
  bool success;
  std::string detail;
  std::vector<Record> data;
};

class DBConnector
{
public:
  DBConnector(const std::string &host = "127.0.0.1", const std::string &port = "65535")
  {
    client_.setOnMessage([this](const std::string &msg)
                         {
    std::lock_guard<std::mutex> lk(mux_);
    lastMsg_ = msg;
    cv_.notify_one(); });
  };
  ~DBConnector()
  {
    disconnect();
  };
  void connect()
  {
    client_.connectToServer();
  };
  void disconnect()
  {
    client_.disconnect();
  };

  DBResponse query(const std::string &qry)
  {
    return sendCommand(qry);
  };
  DBResponse cmd(const std::string &c)
  {
    return sendCommand(c);
  };
  DBResponse get(const std::string &table, int64_t key)
  {
    return sendCommand("GET " + table + " " + std::to_string(key));
  };
  DBResponse create(const std::string &table, int keyCol,
                    const std::vector<std::tuple<std::string, std::string>> &values)
  {
    validateKinds(values);
    std::ostringstream cmd;
    cmd << "CREATE " << table << " " << keyCol;
    for (auto &t : values)
    {
      cmd << " " << std::get<0>(t) << ":" << std::get<1>(t);
    }
    return sendCommand(cmd.str());
  };
  DBResponse insert(const std::string &table,
                    const std::vector<std::tuple<std::string, std::string, std::string>> &values)
  {
    validateKinds(values);
    std::ostringstream cmd;
    cmd << "INSERT " << table;
    for (auto &t : values)
    {
      auto &col = std::get<0>(t);
      auto &val = std::get<1>(t);
      auto &kind = std::get<2>(t);
      if (val.find(' ') != std::string::npos)
        cmd << " " << col << ":\"" << val << "\":" << kind;
      else
        cmd << " " << col << ":" << val << ":" << kind;
    }
    return sendCommand(cmd.str());
  };
  DBResponse update(const std::string &table, int64_t key,
                    const std::vector<std::tuple<std::string, std::string, std::string>> &values)
  {
    validateKinds(values);
    std::ostringstream cmd;
    cmd << "UPDATE " << table << " " << key;
    for (auto &t : values)
    {
      auto &col = std::get<0>(t);
      auto &val = std::get<1>(t);
      auto &kind = std::get<2>(t);
      if (val.find(' ') != std::string::npos)
        cmd << " " << col << ":\"" << val << "\":" << kind;
      else
        cmd << " " << col << ":" << val << ":" << kind;
    }
    return sendCommand(cmd.str());
  };
  DBResponse remove(const std::string &table, int64_t key)
  {
    std::ostringstream cmd;
    cmd << "DELETE " << table << " " << key;
    return sendCommand(cmd.str());
  };

private:
  AsyncClient client_;
  std::string lastMsg_;
  mutable std::mutex mux_;
  std::condition_variable cv_;

  DBResponse sendCommand(const std::string &cmd)
  {
    {
      std::lock_guard<std::mutex> lk(mux_);
      lastMsg_.clear();
    }
    client_.sendMessage(cmd);
    std::unique_lock<std::mutex> lk(mux_);
    cv_.wait(lk, [this]
             { return !lastMsg_.empty(); });
    return parseJson(lastMsg_);
  };
  DBResponse parseJson(const std::string &j)
  {
    DBResponse r;
    auto b = j.find('{'), e = j.rfind('}');
    if (b == std::string::npos || e == std::string::npos || e < b)
      throw std::runtime_error("Malformed response");
    auto s = j.substr(b, e - b + 1);
    auto p = s.find("\"success\":");
    p = (p == std::string::npos ? throw std::runtime_error("Missing success") : p + 10);
    auto endp = s.find_first_of(",}", p);
    r.success = (s.substr(p, endp - p) == "true");
    auto d0 = s.find("\"detail\":");
    if (d0 != std::string::npos)
    {
      auto q1 = s.find('"', d0 + 9), q2 = s.find('"', q1 + 1);
      r.detail = s.substr(q1 + 1, q2 - q1 - 1);
    }
    auto a0 = s.find("\"data\":");
    if (a0 != std::string::npos)
    {
      auto arrStart = s.find('[', a0);
      auto arrEnd = s.find(']', arrStart);
      if (arrStart != std::string::npos && arrEnd != std::string::npos && arrEnd > arrStart)
      {
        std::string arr = s.substr(arrStart + 1, arrEnd - arrStart - 1);
        size_t pos = 0;
        while ((pos = arr.find('{', pos)) != std::string::npos)
        {
          auto end = arr.find('}', pos);
          std::string obj = arr.substr(pos + 1, end - pos - 1);
          Record rec;
          std::istringstream ss(obj);
          std::string field;
          while (std::getline(ss, field, ','))
          {
            auto keyStart = field.find('"');
            auto keyEnd = field.find('"', keyStart + 1);
            std::string key = field.substr(keyStart + 1, keyEnd - keyStart - 1);
            auto colon = field.find(':', keyEnd);
            auto val1 = field.find('"', colon + 1);
            auto val2 = field.find('"', val1 + 1);
            std::string val = field.substr(val1 + 1, val2 - val1 - 1);
            rec[key] = val;
          }
          r.data.push_back(std::move(rec));
          pos = end + 1;
        }
      }
    }
    return r;
  }

  void validateKinds(
      const std::vector<std::tuple<std::string, std::string, std::string>> &values)
  {
    for (auto &t : values)
    {
      auto &kind = std::get<2>(t);
      if (kind != "INT" && kind != "TEXT" && kind != "DOUBLE" && kind != "DATE")
        throw std::invalid_argument("Invalid column type: " + kind);
    }
  }

  void validateKinds(
      const std::vector<std::tuple<std::string, std::string>> &values)
  {
    for (auto &t : values)
    {
      auto &kind = std::get<1>(t);
      if (kind != "INT" && kind != "TEXT" && kind != "DOUBLE" && kind != "DATE")
        throw std::invalid_argument("Invalid column type: " + kind);
    }
  }
};
