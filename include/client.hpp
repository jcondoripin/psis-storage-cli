#pragma once

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socklen_t = int;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
using SOCKET = int;
#endif

#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <functional>

class AsyncClient
{
private:
  std::string host_;
  std::string port_;
  SOCKET sock_ = INVALID_SOCKET;
  std::thread recvThread_;
  std::atomic<bool> running_{false};
  std::function<void(const std::string &)> onMessage_;

public:
  AsyncClient(const std::string &host = "127.0.0.1", const std::string &port = "65535")
      : host_(host), port_(port)
  {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      throw std::runtime_error("WSAStartup failed");
    }
#endif
  }

  ~AsyncClient()
  {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
  }

  void connectToServer()
  {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host_.c_str(), port_.c_str(), &hints, &res) != 0)
    {
      throw std::runtime_error("getaddrinfo failed");
    }

    for (auto p = res; p; p = p->ai_next)
    {
      sock_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sock_ == INVALID_SOCKET)
        continue;
      if (connect(sock_, p->ai_addr, (int)p->ai_addrlen) != SOCKET_ERROR)
        break;

#ifdef _WIN32
      closesocket(sock_);
#else
      close(sock_);
#endif
      sock_ = INVALID_SOCKET;
    }

    freeaddrinfo(res);

    if (sock_ == INVALID_SOCKET)
    {
      throw std::runtime_error("Unable to connect to server at " + host_ + ":" + port_);
    }

    std::cout << "Connected to " << host_ << ":" << port_ << "\n";

    running_ = true;
    recvThread_ = std::thread([this]()
                              { this->receiveLoop(); });
  }

  void disconnect()
  {
    if (sock_ != INVALID_SOCKET)
    {
#ifdef _WIN32
      shutdown(sock_, SD_BOTH);
      closesocket(sock_);
#else
      shutdown(sock_, SHUT_RDWR);
      close(sock_);
#endif
      sock_ = INVALID_SOCKET;
    }

    running_ = false;

    if (recvThread_.joinable())
      recvThread_.join();
  }

  void sendMessage(const std::string &message)
  {
    if (sock_ == INVALID_SOCKET)
      throw std::runtime_error("Not connected");

#ifdef _WIN32
    int sent = send(sock_, message.c_str(), (int)message.size(), 0);
#else
    ssize_t sent = ::send(sock_, message.c_str(), message.size(), 0);
#endif
    if (sent == SOCKET_ERROR)
    {
      throw std::runtime_error("Send failed");
    }
  }

  void setOnMessage(std::function<void(const std::string &)> handler)
  {
    onMessage_ = std::move(handler);
  }

private:
  void receiveLoop()
  {
    constexpr size_t MAXBUF = 1024;
    std::vector<char> buf(MAXBUF);

    while (running_)
    {
#ifdef _WIN32
      int recvd = recv(sock_, buf.data(), (int)buf.size() - 1, 0);
#else
      ssize_t recvd = ::recv(sock_, buf.data(), buf.size() - 1, 0);
#endif

      if (recvd > 0)
      {
        buf[recvd] = '\0';
        if (onMessage_)
        {
          onMessage_(std::string(buf.data()));
        }
      }
      else if (recvd == 0)
      {
        std::cout << "Server closed connection\n";
        running_ = false;
        break;
      }
      else
      {
        std::cerr << "Receive error\n";
        running_ = false;
        break;
      }
    }
  }
};
