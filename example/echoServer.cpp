#include "simpleSock.h"

const int BUFFER_SIZE = 512;

void LogError(std::string msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Server {
private:
    ssock::Socket m_listenSock, m_clientSock;
    char m_buffer[BUFFER_SIZE];

public:
    Server() 
        : m_listenSock(ssock::ProtocolType::TCP), 
          m_clientSock(ssock::ProtocolType::TCP) {}
    ~Server() {}

    errcode_t BindAndListen(std::string addrIn, uint16_t port) {
        std::cout << "Will try to bind a server at " << port << " port\n";
        if (m_listenSock.Bind(ssock::Address(addrIn, port)) == SOCKET_ERROR) 
            return SOCKET_ERROR;
        if (m_listenSock.Listen(64) == SOCKET_ERROR) 
            return SOCKET_ERROR;

        ssock::Address addr; 
        m_listenSock.GetSockAddress(addr);
        std::cout << "Server started listening at address: " << addr.GetFullAddress() << '\n';
        return SUCCESS;
    }

    errcode_t Accept() {
        ssock::Address addr;
        ssock::Socket acceptedSock(m_listenSock.Accept());
        if (acceptedSock.GetSocket() == INVALID_SOCKET)
            return SOCKET_ERROR;
        m_clientSock = std::move(acceptedSock);

        std::cout << "\nAccepted client!\n";
        m_clientSock.GetSockAddress(addr);
        std::cout << "Local client sock address  = " << addr.GetFullAddress() << '\n';
        m_clientSock.GetPeerAddress(addr);
        std::cout << "Remote client sock address = " << addr.GetFullAddress() << '\n';
        return SUCCESS;
    }

    void Receive() {
        memset(m_buffer, 0, BUFFER_SIZE);
        int readBytes = m_clientSock.Read(m_buffer, BUFFER_SIZE);
        std::cout << "Received " << readBytes << " bytes: " << m_buffer << '\n';
    }

    void Reply() {
        #ifdef _WIN32
            std::strcat(m_buffer, " --- WINDOWS echo server");
        #else 
            std::strcat(m_buffer, " --- UNIX echo server");
        #endif
        int sentBytes = m_clientSock.Write(m_buffer, strlen(m_buffer));
        std::cout << "Sent " << sentBytes << " bytes: " << m_buffer << '\n';
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
        if (server.BindAndListen("0.0.0.0", port) == SOCKET_ERROR) {
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
