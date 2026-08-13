#include "simpleSock.h"
#include <array>

const size_t BUFFER_SIZE = 512;

void LogError(const std::string &msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Client {
private:
    ssock::Socket m_sock;
    std::array<char, BUFFER_SIZE> m_buffer;

public:
    Client() : m_sock(ssock::ProtocolType::TCP) {}
    ~Client() {}

    errcode_t Connect(const ssock::Address &serverAddr) {
        std::cout << "Will try to connect to " << serverAddr.GetFullAddress() << '\n'; 
        if (m_sock.Connect(serverAddr) == SOCKET_ERROR) return SOCKET_ERROR;
        std::cout << "Connected to server!\n";
        ssock::Address addr;
        m_sock.GetSockAddress(addr);
        std::cout << "Local client sock address  = " << addr.GetFullAddress() << '\n';
        m_sock.GetPeerAddress(addr);
        std::cout << "Remote server sock address = " << addr.GetFullAddress() << '\n';
        return SUCCESS;
    }

    void Receive() {
        m_buffer.fill(0);
        int readBytes = m_sock.Read(m_buffer.data(), BUFFER_SIZE);
        std::cout << "Received " << readBytes << " bytes: " << m_buffer.data() << '\n';
    }

    void Send(const std::string &word) {
        int sentBytes = m_sock.Write(word.c_str(), word.size());
        std::cout << "Sent " << sentBytes << " bytes: " << word.substr(0, sentBytes) << '\n';
    }
};

int main(int argc, char* argv[]) {
    std::string addr = "127.0.0.1";
    uint16_t    port = 8080;
    if (argc > 1) {
        addr = argv[1];
        if (argc > 2) port = std::atoi(argv[2]);
    }
    ssock::WinStartup();
    {
        Client client;
        if (client.Connect(ssock::Address(addr, port)) == SOCKET_ERROR) {
            LogError("Failed to connect to remote server!");
            return static_cast<errcode_t>(ssock::GetLastError());
        }
        #ifdef _WIN32
            client.Send("Hello from .#-WINDOWS-#. client!");
        #else 
            client.Send("Hello from !__UNIX__! client!");
        #endif
        client.Receive();
    }
    ssock::WinCleanup();
}
