#include "simpleSock.h"
#include <array>

const size_t BUFFER_SIZE = 512,
             BACKLOG_SIZE = 64;

void LogError(const std::string &msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Server {
private:
    size_t m_bufferSize;
    ssock::Socket m_sock;
    ssock::Address m_clientAddr;
    std::array<char, BUFFER_SIZE> m_buffer;

public:
    Server() : m_sock(ssock::ProtocolType::UDP) {}
    ~Server() {}

    errcode_t Bind(const ssock::Address &addrIn) {
        std::cout << "Will try to bind a server at " << addrIn.GetPort() << '\n';
        if (m_sock.Bind(addrIn) == SOCKET_ERROR)
            return SOCKET_ERROR;
        std::cout << "Server started at address: " << addrIn.GetFullAddress() << '\n';
        return SUCCESS;
    }

    void Receive() {
        m_buffer.fill(0);
        int readBytes = m_sock.Read(m_buffer.data(), BUFFER_SIZE, m_clientAddr);
        std::cout << "Source of data: " << m_clientAddr.GetFullAddress() << '\n';
        std::cout << "Received " << readBytes << " bytes: " << m_buffer.data() << '\n';
        m_bufferSize = readBytes;
    }

    void BufferConcatenateString(const std::string &str) {
        memcpy(m_buffer.data()+m_bufferSize, str.data(), str.size());
        m_bufferSize += str.size();
    }

    void Reply() {
        #ifdef _WIN32
            BufferConcatenateString(" --- WINDOWS echo server");
        #else 
            BufferConcatenateString(" --- UNIX echo server");
        #endif
        int sentBytes = m_sock.Write(m_buffer.data(), m_bufferSize, m_clientAddr);
        std::cout << "Target client address: " << m_clientAddr.GetFullAddress() << '\n';
        std::cout << "Sent " << sentBytes << " bytes: " << m_buffer.data() << '\n';
    }

};

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) port = std::atoi(argv[1]);
    ssock::WinStartup();
    {
        Server server;
        if (server.Bind(ssock::Address("0.0.0.0", port)) == SOCKET_ERROR) {
            LogError("Failed to bind server to specified port");
            return static_cast<errcode_t>(ssock::GetLastError());
        }
        while (true) {
            server.Receive();
            server.Reply();
        }
    }
    ssock::WinCleanup();
}
