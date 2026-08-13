#include "simpleSock.h"
#include <csignal>
#include <thread>
#include <chrono>
#include <array>

const size_t BUFFER_SIZE = 512,
             BACKLOG_SIZE = 64;
const int SERVER_LOOP_TIME = 1000; // in ms

namespace timer = std::chrono;

void LogError(const std::string &msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Server {
private:
    size_t m_bufferSize;
    ssock::Poll   m_poll;
    ssock::Socket m_listenSock, m_clientSock;
    timer::time_point<timer::high_resolution_clock> m_lastTime,
                                                    m_timeSinceStart;
    std::array<char, BUFFER_SIZE> m_buffer;
    bool m_canReply, m_canAccept;

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

        m_poll.AddMonitor(m_clientSock.GetSocket(), ssock::EventType::ReadReady |
                                                    ssock::EventType::WriteReady);
        std::cout << "Added client to poll queue\n\n";
        m_canAccept = false;
        return SUCCESS;
    }


    void Receive() {
        m_buffer.fill(0);
        int readBytes = m_clientSock.Read(m_buffer.data(), BUFFER_SIZE);
        std::cout << "Received " << readBytes << " bytes: " << m_buffer.data() << '\n';
        m_bufferSize = readBytes;
        m_canReply = true;
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
        m_canReply = false;
    }

    void DisconnectClient() {
        m_poll.DeleteMonitor(m_clientSock.GetSocket());
        m_clientSock.Shutdown(ssock::ShutdownType::BOTH);
        m_clientSock.Close();
        m_canAccept = true;
    }

public:
    Server() 
        : m_listenSock(ssock::ProtocolType::TCP), 
          m_clientSock(ssock::ProtocolType::TCP),
          m_timeSinceStart(timer::high_resolution_clock::now()),
          m_canReply(false), m_canAccept(true) {}
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
        if (m_listenSock.IsBlocking()) {
            if (m_listenSock.SwitchBlockingState() == SOCKET_ERROR) 
                return SOCKET_ERROR;
        }
        if (m_clientSock.IsBlocking()) {
            if (m_clientSock.SwitchBlockingState() == SOCKET_ERROR) 
                return SOCKET_ERROR;
        }
        std::cout << "Made sockets non blocking\n";
        m_poll.AddMonitor(m_listenSock.GetSocket(), ssock::EventType::ReadReady |
                                                    ssock::EventType::WriteReady);
        std::cout << "Added listening socket to poll queue\n";
        return SUCCESS;
    }

    void PollSockets() {
        m_lastTime = timer::high_resolution_clock::now();
        ssize_t readyMonitorsCount = m_poll.WaitForReadiness(0);
        std::cout << "Ready monitors count = " << readyMonitorsCount << '\n';

        std::vector<ssock::pollfde_t> readyMonitors = 
            m_poll.GetReadyMonitors(readyMonitorsCount);

        for (const auto &monitor : readyMonitors) {
            auto socket = monitor.fd;
            auto &pendingEvent = monitor.revents;
            if (socket == m_listenSock.GetSocket()) {
                if (m_canAccept)
                    if (Accept() == SOCKET_ERROR)
                        LogError("Failed to accept connection");
            } else if ((pendingEvent & ssock::EventType::ConnectionClosed) ||
                       (pendingEvent & ssock::EventType::InvalidSocket) ||
                       (pendingEvent & ssock::EventType::ErrorOccured))
                DisconnectClient();
            else if (pendingEvent & ssock::EventType::ReadReady) 
                Receive();
            else if (pendingEvent & ssock::EventType::WriteReady) {
                if (m_canReply) {
                    Reply();
                    DisconnectClient();
                }
            }
        }

    }

    void SleepUntilNextIteration() {
        auto curTime = timer::high_resolution_clock::now();
        auto deltaTime = curTime - m_lastTime;
        auto sleepTime = timer::milliseconds(SERVER_LOOP_TIME) - 
                         timer::duration_cast<timer::milliseconds>(deltaTime);
        auto deltaTimeSinceStart = curTime - m_timeSinceStart;
        
        std::cout << "Server uptime: " 
                  << timer::duration_cast<timer::milliseconds>(deltaTimeSinceStart).count() 
                  << " ms\n";
        if (sleepTime > timer::milliseconds(0)) std::this_thread::sleep_for(sleepTime);
    }
};

volatile sig_atomic_t g_isRunning = 1;
void SIGINTCallback(int sig) {g_isRunning = 0;}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, SIGINTCallback);
    uint16_t port = 8080;
    if (argc > 1) port = std::atoi(argv[1]);
    ssock::WinStartup();
    {
        Server server;
        if (server.BindAndListen(ssock::Address("0.0.0.0", port)) == SOCKET_ERROR) {
            LogError("Failed to start a server");
            return static_cast<errcode_t>(ssock::GetLastError());
        }
        while (g_isRunning) {
            server.PollSockets();
            server.SleepUntilNextIteration();
        }
    }
    ssock::WinCleanup();
    std::cout << "Server shutdown\n";
}
