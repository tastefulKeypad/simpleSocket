#include "simpleSock.h"
#include <array>
#include <thread>
#include <chrono>

namespace timer = std::chrono;

const size_t BUFFER_SIZE = 512;


void LogError(const std::string &msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Client {
private:
    size_t m_bufferSize;
    ssock::Socket m_sock;
    ssock::Address m_addr;
    std::array<char, BUFFER_SIZE> m_buffer;
    ssock::Poll m_poll;

public:
    Client(const ssock::Address &addrIn) : 
        m_sock(ssock::ProtocolType::UDP), 
        m_addr(addrIn) {}
    ~Client() {}

    void Receive() {
        m_buffer.fill(0);
        int readBytes = m_sock.Read(m_buffer.data(), BUFFER_SIZE, m_addr);
        std::cout << "Received " << readBytes << " bytes: " << m_buffer.data() << '\n';
        m_bufferSize = readBytes;
    }

    void Send(const std::string &word) {
        std::cout << "Will try to send message to: " << m_addr.GetFullAddress() << '\n';
        int sentBytes = m_sock.Write(word.c_str(), word.size(), m_addr);
        std::cout << "Sent " << sentBytes << " bytes: " << word.substr(0, sentBytes) << '\n';
    }

    void UpdateTargetAddress() {
        std::string newAddr(m_buffer.data(), m_bufferSize);
        uint16_t newPort;
        auto it = newAddr.find(':')+1;
        std::cout << "SUBSTR = " << newAddr.substr(it, newAddr.size()) << '\n';
        newPort = std::stoi(newAddr.substr(it, newAddr.size()));
        newAddr = newAddr.substr(0, it);
        m_addr = ssock::Address(newAddr, newPort);
    }

    bool TryToConnectToPeer(int repeatCount) {
        m_sock.SwitchBlockingState();
        m_poll.AddMonitor(m_sock.GetSocket(), ssock::EventType::ReadReady);
        while (--repeatCount >= 0) {
            m_sock.Write("HELLO PEER", 10, m_addr);
            ssize_t readyMonitorCount = m_poll.WaitForReadiness(500);
            if (readyMonitorCount <= 0) continue;
            auto readyMonitors = m_poll.GetReadyMonitors(size_t(readyMonitorCount));
            auto &pendingEvent = readyMonitors.back().revents;
            if (pendingEvent & ssock::EventType::ReadReady) {
                ssock::Address addrIn;
                m_buffer.fill(0);
                int readBytes = m_sock.Read(m_buffer.data(), BUFFER_SIZE, addrIn);
                std::cout << "Received " << readBytes << " bytes: " << m_buffer.data() << '\n';
                std::cout << "Source address = " << addrIn.GetFullAddress() << '\n';
                m_bufferSize = readBytes;
                if (addrIn == m_addr) return true;
            }
        }
        return false;
    }
};

int main(int argc, char* argv[]) {
    std::string addr = "127.0.0.1",
                msg  = "HOST";
    uint16_t    port = 8080;
    if (argc > 1) {
        addr = argv[1];
        if (argc > 2) port = std::atoi(argv[2]);
        if (argc > 3) msg = argv[3];
    }
    ssock::WinStartup();
    {
        Client client(ssock::Address(addr, port));
        client.Send(msg);
        client.Receive();
        client.UpdateTargetAddress();
        if (client.TryToConnectToPeer(5)) {
            if (msg == "HOST")
                client.Send("Nice to meet you! I am - HOST!");
            else
                client.Send("Nice to meet you! I am - CLIENT!");
            client.Receive();
        } else std::cout << "Failed to perform UDP hole punching :(\n";
    }
    ssock::WinCleanup();
}
