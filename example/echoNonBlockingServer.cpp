#include "simpleSock.h"
#include <csignal>
#include <thread>
#include <chrono>

const int BUFFER_SIZE = 512;
const int SERVER_LOOP_TIME = 1000; // in ms

namespace timer = std::chrono;

void LogError(std::string msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

class Server {
private:
    ssock::Poll   m_poll;
    ssock::Socket m_listenSock, m_clientSock;
    timer::time_point<timer::high_resolution_clock> m_lastTime,
                                                    m_timeSinceStart;
                                                    
    bool canReply = false, canAccept = true;
    char buffer[BUFFER_SIZE];

    void Receive() {
        memset(buffer, 0, BUFFER_SIZE);
        int readBytes = m_clientSock.Read(buffer, BUFFER_SIZE);
        std::cout << "Received " << readBytes << " bytes: " << buffer << '\n';
        canReply = true;
    }

    void Reply() {
        #ifdef _WIN32
            std::strcat(buffer, " --- WINDOWS echo server");
        #else 
            std::strcat(buffer, " --- UNIX echo server");
        #endif
        int sentBytes = m_clientSock.Write(buffer, strlen(buffer));
        std::cout << "Sent " << sentBytes << " bytes: " << buffer << '\n';
        canReply = false;
    }

    void DisconnectClient() {
        m_poll.DeleteMonitor(m_clientSock.GetSocket());
        m_clientSock.Shutdown(ssock::ShutdownType::BOTH);
        m_clientSock.Close();
        canAccept = true;
    }

public:
    Server() 
        : m_listenSock(ssock::ProtocolType::TCP), 
          m_clientSock(ssock::ProtocolType::TCP),
          m_timeSinceStart(timer::high_resolution_clock::now()) {}
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
        canAccept = false;
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
            auto pendingEvent = monitor.revents;
            if (socket == m_listenSock.GetSocket()) {
                if (canAccept)
                    if (Accept() == SOCKET_ERROR)
                        LogError("Failed to accept connection");
            } else if ((pendingEvent & ssock::EventType::ConnectionClosed) ||
                       (pendingEvent & ssock::EventType::InvalidSocket) ||
                       (pendingEvent & ssock::EventType::ErrorOccured))
                DisconnectClient();
            else if (pendingEvent & ssock::EventType::ReadReady) Receive();
            else if (pendingEvent & ssock::EventType::WriteReady) {
                if (canReply) {
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
        if (server.BindAndListen("0.0.0.0", port) == SOCKET_ERROR) {
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
