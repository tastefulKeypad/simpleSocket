#include "simpleSock.h"
#include <array>

const size_t BUFFER_SIZE = 512;

void LogError(const std::string &msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Client {
private:
    size_t m_bufferSize;
    ssock::Socket m_sock;
    ssock::Address m_serverAddr;
    std::array<char, BUFFER_SIZE> m_buffer;

public:
    Client(const ssock::Address &serverAddr) : 
        m_sock(ssock::ProtocolType::UDP), 
        m_serverAddr(serverAddr) {}
    ~Client() {}

    void Receive() {
        m_buffer.fill(0);
        int readBytes = m_sock.Read(m_buffer.data(), BUFFER_SIZE, m_serverAddr);
        std::cout << "Received " << readBytes << " bytes: " << m_buffer.data() << '\n';
    }

    void Send(const std::string &word) {
        std::cout << "Will try to send message to server at: " << m_serverAddr.GetFullAddress() << '\n';
        int sentBytes = m_sock.Write(word.c_str(), word.size(), m_serverAddr);
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
        Client client(ssock::Address(addr, port));
        #ifdef _WIN32
            client.Send("Hello from .#-WINDOWS-#. client!");
        #else 
            client.Send("Hello from !__UNIX__! client!");
        #endif
        client.Receive();
    }
    ssock::WinCleanup();
}
