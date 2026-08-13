#include "simpleSock.h"
#include <csignal>
#include <vector>
#include <array>
#include <algorithm>
#include <string_view>

const size_t BUFFER_SIZE = 128;

class RelayServer {
private:
    size_t m_bufferSize;
    ssock::Socket m_sock;
    std::array<char, BUFFER_SIZE> m_buffer;
    std::vector<ssock::Address> m_hosts;

public:
    RelayServer() 
        : m_sock(ssock::ProtocolType::UDP) {}
    ~RelayServer() {}

    errcode_t Bind(const ssock::Address &addrIn) {
        std::cout << "Will try to bind server at " << addrIn.GetPort() << " port\n";
        if (m_sock.Bind(addrIn) == SOCKET_ERROR)
            return SOCKET_ERROR;
        std::cout << "Started relay server at address: " <<  addrIn.GetFullAddress() << '\n';
        return SUCCESS;
    }
    void ProcessClients() {
        m_buffer.fill(0);
        ssock::Address clientAddr;
        int readBytes = m_sock.Read(m_buffer.data(), m_buffer.size(), clientAddr);
        m_bufferSize = readBytes;
        std::cout << "Accepted data from " << clientAddr.GetFullAddress() << " = " << m_buffer.data() << '\n';

        // Ignore repeating hosts
        if (std::find(m_hosts.begin(), m_hosts.end(), clientAddr) != m_hosts.end()) 
            return;

        // Add to host vector
        std::string_view clientData{m_buffer.data(), m_bufferSize};
        if (clientData == "HOST") {
            m_hosts.push_back(clientAddr);
            return;
        }

        // Check if we have specified host registered
        ssock::Address hostAddr(std::string(clientData), uint16_t(0));
        ssize_t hostId = -1;
        for (size_t i = 0; i != m_hosts.size(); ++i) {
            if (m_hosts[i].GetAddress() == hostAddr.GetAddress()) {
                hostId = i;
                break;
            }
        }
        if (hostId == -1) return;

        // Tell host/client to start UDP punch hole
        hostAddr = m_hosts[hostId];
        m_hosts.erase(m_hosts.begin()+hostId);
        //for now, just notify them about connection
        std::string clientAddrStr = clientAddr.GetFullAddress(),
                    hostAddrStr   = hostAddr.GetFullAddress();

        //clientAddrStr = "yeah, go on";
        //hostAddrStr = "yeah, go on";

        m_sock.Write(clientAddrStr.data(), clientAddrStr.size(), hostAddr);
        m_sock.Write(hostAddrStr.data(), hostAddrStr.size(), clientAddr);
    }
};

volatile sig_atomic_t g_isRunning = 1;
void SIGINTCallback(int sig) {g_isRunning = 0;}

int main(int argc, char* argv[]) {
    //std::signal(SIGINT, SIGINTCallback);
    uint16_t port = 8080;
    if (argc > 1) port = std::atoi(argv[1]);
    ssock::WinStartup();
    {
        RelayServer relay;
        if (relay.Bind(ssock::Address("0.0.0.0", port)) == SOCKET_ERROR) {
            std::cout << "Failed to start relay server\n";
            std::cout << "Reason: " << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
            return static_cast<errcode_t>(ssock::GetLastError());
        }
        while (g_isRunning) {
            relay.ProcessClients();
        }
    }
    ssock::WinCleanup();
}
