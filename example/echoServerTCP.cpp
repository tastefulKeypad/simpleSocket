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
    ssock::Socket m_listenSock, m_clientSock;
    std::array<char, BUFFER_SIZE> m_buffer;

public:
    Server() 
        : m_bufferSize(0),
          m_listenSock(ssock::ProtocolType::TCP), 
          m_clientSock(ssock::ProtocolType::TCP) {}
    ~Server() {}

    errcode_t BindAndListen(const ssock::Address &addrIn) {
        std::cout << "Will try to bind a server at " << addrIn.GetPort() << " port\n";
        if (m_listenSock.Bind(addrIn) == SOCKET_ERROR) 
            return SOCKET_ERROR;
        if (m_listenSock.Listen(BACKLOG_SIZE) == SOCKET_ERROR) 
            return SOCKET_ERROR;

        ssock::Address addr; 
        m_listenSock.GetSockAddress(addr);
        std::cout << "Server started listening at address: " << addr.GetFullAddress() << '\n';
        return SUCCESS;
    }

    errcode_t Accept() {
        ssock::Socket acceptedSock(m_listenSock.Accept());
        if (acceptedSock.GetSocket() == INVALID_SOCKET)
            return SOCKET_ERROR;
        m_clientSock = std::move(acceptedSock);

        ssock::Address addr;
        std::cout << "\nAccepted client!\n";
        m_clientSock.GetSockAddress(addr);
        std::cout << "Local client sock address  = " << addr.GetFullAddress() << '\n';
        m_clientSock.GetPeerAddress(addr);
        std::cout << "Remote client sock address = " << addr.GetFullAddress() << '\n';
        return SUCCESS;
    }

    void Receive() {
        m_buffer.fill(0);
        int readBytes = m_clientSock.Read(m_buffer.data(), BUFFER_SIZE);
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
        int sentBytes = m_clientSock.Write(m_buffer.data(), m_bufferSize);
        std::cout << "Sent " << sentBytes << " bytes: " << m_buffer.data() << '\n';
    }

    void DisconnectClient() {
        m_clientSock.Shutdown(ssock::ShutdownType::BOTH);
        m_clientSock.Close();
    }
};

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) port = std::atoi(argv[1]);
    ssock::WinStartup();
    {
        Server server;
        if (server.BindAndListen(ssock::Address("0.0.0.0", port)) == SOCKET_ERROR) {
            LogError("Failed to start a server");
            return static_cast<errcode_t>(ssock::GetLastError());
        }
        while (true) {
            if (server.Accept() != SOCKET_ERROR) {
                server.Receive();
                server.Reply();
                server.DisconnectClient();
            } else LogError("Failed to accept client connection");
        }
    }
    ssock::WinCleanup();
}
