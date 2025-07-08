#include "include/client.hpp"
#include <iostream>
#include <cstring>

int main(int argc, char *argv[])
{
  std::string host = "127.0.0.1";
  uint16_t port = 65535;

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "-h") == 0 && i + 1 < argc)
    {
      host = argv[++i];
    }
    else if (std::strcmp(argv[i], "-p") == 0 && i + 1 < argc)
    {
      port = static_cast<uint16_t>(std::stoi(argv[++i]));
    }
    else
    {
      std::cerr << "Uso: " << argv[0] << " [-h <host>] [-p <port>]\n";
      return 1;
    }
  }

  try
  {
    AsyncClient client;

    client.setOnMessage([](const std::string &msg)
                        {
            std::cout << "\n[Server]: " << msg << "\n> ";
            std::cout.flush(); });

    std::cout << "[Config]: Conectando a " << host << ":" << port << "\n";

    client.connectToServer();

    std::string line;
    while (true)
    {
      std::cout << "> ";
      if (!std::getline(std::cin, line))
        break;
      if (line == "exit")
        break;
      client.sendMessage(line);
    }

    client.disconnect();
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Error: " << ex.what() << "\n";
  }

  return 0;
}
