#include "simpleSock.h"
#include "chatCommon.h"
#include <csignal>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <bitset>

const int SERVER_LOOP_TIME = 100, // in ms  
          CONNECT_TIMEOUT = 5000, // in ms
          STATUS_NOTIFICATION_TIME = 2000, // in ms
          MIN_CLIENTS = 2,
          MAX_CLIENTS = 30;
          
namespace timer = std::chrono;

class ChatServer {
private:
    int           m_maxClients, m_listenMonitorId;
    std::string   m_name;
    ssock::Poll   m_poll;
    ssock::Socket m_listenSocket;
    timer::time_point<timer::high_resolution_clock> m_lastTime,
                                                    m_lastTimeNotifiedOfStatus,
                                                    m_timeSinceStart;
    std::vector<char>                  m_buffer;
    std::vector<Session>               m_clients;
    std::vector<Packet>                m_outcomingPackets;
    std::vector<ssock::pollfde_t>      m_readyMonitors;

    void ReadClientData(Session &client) {
        memset(m_buffer.data(), 0, m_buffer.size());
        int readBytes = client.m_sock.Read(m_buffer.data(), m_buffer.size());
        if (readBytes == 0) {
            std::cout << "USER DISCONNECTED! will disconnect user: " << client.m_sock.GetSocket() << '\n';
            client.m_shouldDisconnect = true;
            return;
        }
        PacketType packetType;
        uint16_t bodySize;
        std::vector<char> body;
        DeserializePacket(m_buffer, packetType, bodySize, body);

        if (packetType == PacketType::CONNECT) {
            if (client.m_isConnected) return;
            std::string name;
            GetNameFromBody(body, name);
            std::cout << "Client = " << client.m_sock.GetSocket() << " CONNECT: "
                      << name << '\n';
            if (name == "SERVER" || name == "SYSTEM") {
                std::cout << "BAD NAME! will disconnect user: " << client.m_sock.GetSocket() << '\n';
                client.m_shouldDisconnect = true;
                return;
            }
            client.m_name = name;
            client.m_isConnected = true;
            std::string msg = name+" has connected!";
            m_outcomingPackets.push_back(MakeMessagePacket(m_name, msg));
        } else if (packetType == PacketType::MESSAGE) {
            if (!client.m_isConnected) return;
            std::string name, msg;
            GetNameFromBody(body, name);
            GetMessageFromBody(body, msg);
            std::cout << "Client = " << client.m_sock.GetSocket() << " MESSAGE: "
                      << name << " | " << msg << '\n';
            m_outcomingPackets.push_back(MakeMessagePacket(client.m_name, msg));
        } else if (readBytes){
            std::cout << "BAD DATA! will disconnect user: " << client.m_sock.GetSocket() << '\n';
            client.m_shouldDisconnect = true;
        }
    }
    void WriteClientData(Session &client) {
        if (!client.m_isConnected || client.m_shouldDisconnect) return;
        for (const auto &packet : m_outcomingPackets) {
            std::vector<char> serializedPacket = SerializePacket(packet);
            client.m_sock.Write(serializedPacket.data(), serializedPacket.size());
        }
    }
    void FormNotificationPacket() {
        auto curTime = timer::high_resolution_clock::now();
        auto timeDelta = 
            timer::duration_cast<timer::milliseconds>(curTime - m_lastTimeNotifiedOfStatus);
        if (timeDelta > timer::milliseconds(STATUS_NOTIFICATION_TIME)) {
            m_lastTimeNotifiedOfStatus = curTime;
            timeDelta = timer::duration_cast<timer::milliseconds>(curTime - m_timeSinceStart);
            uint16_t clientCount = 0;
            uint32_t uptime = uint32_t(timeDelta.count());
            for (const auto& client : m_clients) {
                if (client.m_isConnected) ++clientCount;
            }
            m_outcomingPackets.push_back(MakeServerStatusPacket(clientCount, uptime));
        }
    }

public:
    ChatServer(int maxClients) : 
        m_maxClients(maxClients),
        m_name("SERVER"),
        m_listenSocket(ssock::ProtocolType::TCP) ,
        m_lastTimeNotifiedOfStatus(timer::high_resolution_clock::now()),
        m_timeSinceStart(timer::high_resolution_clock::now()),
        m_buffer(MAXIMUM_PACKET_SIZE, 0),
        m_readyMonitors(maxClients) {}

    errcode_t BindAndListen(uint16_t port) {
        std::cout << "Will try to bind a server at: " << port << '\n';
        if (m_listenSocket.Bind(ssock::Address("0.0.0.0", port)) == SOCKET_ERROR)
            return SOCKET_ERROR;
        if (m_listenSocket.Listen(m_maxClients) == SOCKET_ERROR)
            return SOCKET_ERROR;
        if (m_listenSocket.IsBlocking()) {
            if (m_listenSocket.SwitchBlockingState() == SOCKET_ERROR)
                return SOCKET_ERROR;
        }
        m_poll.AddMonitor(m_listenSocket.GetSocket(), ssock::EventType::ReadReady);
        ssock::Address addr;
        m_listenSocket.GetSockAddress(addr);
        std::cout << "Successfully started server at: " << addr.GetFullAddress() << '\n';
        return SUCCESS;
    }
    void PollSockets() {
        m_lastTime = timer::high_resolution_clock::now();
        m_readyMonitors.clear();
        ssize_t readyMonitorsCount = m_poll.WaitForReadiness(0);
        if (readyMonitorsCount == SOCKET_ERROR) {
            m_readyMonitors.clear();
            LogError("Failed to poll sockets");
            return;
        }
        m_readyMonitors = m_poll.GetReadyMonitors(readyMonitorsCount);
    }
    void HandleNetworkData() {
        m_listenMonitorId = -1;
        std::unordered_map<size_t, size_t> GetClientId;
        for (size_t i = 0; i != m_clients.size(); ++i)
            GetClientId[m_clients[i].m_sock.GetSocket()] = i;

        // Handle data read & disconnects
        for (size_t i = 0; i != m_readyMonitors.size(); ++i) {
            auto socket = m_readyMonitors[i].fd;
            auto pendingEvent = m_readyMonitors[i].revents;

            if (socket == m_listenSocket.GetSocket()) m_listenMonitorId = int(i);
            else {
                if ((pendingEvent & ssock::EventType::ConnectionClosed) ||
                    (pendingEvent & ssock::EventType::ErrorOccured) ||
                    (pendingEvent & ssock::EventType::InvalidSocket)) {
                    std::cout << "CONNECTION CLOSED/ERROR OCCURED! will disconnect user: " << m_clients[GetClientId[socket]].m_sock.GetSocket() << '\n';
                    m_clients[GetClientId[socket]].m_shouldDisconnect = true;
                } else if (pendingEvent & ssock::EventType::ReadReady)
                    ReadClientData(m_clients[GetClientId[socket]]);
            }
        }
        FormNotificationPacket();

        // Handle data write 
        for (size_t i = 0; i != m_readyMonitors.size(); ++i) {
            auto socket = m_readyMonitors[i].fd;
            auto pendingEvent = m_readyMonitors[i].revents;
            if (socket == m_listenSocket.GetSocket()) continue;
            else {
                if (pendingEvent & ssock::EventType::WriteReady)
                    WriteClientData(m_clients[GetClientId[socket]]);
            }
        }
        m_outcomingPackets.clear();
    }
    void DisconnectBadClients() {
        size_t badClientsCount = 0;
        size_t clientsSize = m_clients.size();
        auto curTime = timer::high_resolution_clock::now();
        for (size_t i = 0; i != clientsSize - badClientsCount; ++i) {
            if (!m_clients[i].m_isConnected) {
                auto deltaTime = 
                    timer::duration_cast<timer::milliseconds>(curTime - m_clients[i].m_startTime);
                if (deltaTime > timer::milliseconds(CONNECT_TIMEOUT)) {
                    std::cout << "TIMEOUT! will disconnect user: " << m_clients[i].m_sock.GetSocket() << '\n';
                    m_clients[i].m_shouldDisconnect = true;
                }
            }
            if (m_clients[i].m_shouldDisconnect) {
                m_poll.DeleteMonitor(m_clients[i].m_sock.GetSocket());
                std::swap(m_clients[i], m_clients[clientsSize - 1 - badClientsCount]);
                ++badClientsCount;
                --i;
            }
        }
        while (badClientsCount > 0) {
            std::cout << "Disconnected client: ";
            if (m_clients.back().m_name.empty()) std::cout << "UNDEFINED_NAME" << '\n';
            else {
                std::cout << m_clients.back().m_name << '\n';
                std::string msg = m_clients.back().m_name+" has disconnected!";
                m_outcomingPackets.push_back(MakeMessagePacket(m_name, msg));
            }
            m_clients.pop_back();
            --badClientsCount;
        }
    }
    void AcceptClients() {
        if (m_listenMonitorId >= 0 && (m_readyMonitors[m_listenMonitorId].revents & ssock::EventType::ReadReady)) {
            m_clients.emplace_back(m_listenSocket.Accept());
            if (m_clients.back().m_sock.GetSocket() == INVALID_SOCKET) {
                LogError("Failed to accept connection from client");
                m_clients.pop_back();
                return;
            }

            ssock::Address addr;
            m_clients.back().m_sock.GetPeerAddress(addr);
            std::cout << "Accepted client from: " << addr.GetFullAddress() << '\n';

            if (m_clients.size() > size_t(m_maxClients)) {
                std::string disconnectReason = "Connection refused: chatroom is full!";
                std::vector<char> serializedPacket = SerializePacket(MakeMessagePacket(m_name, disconnectReason));
                m_clients.back().m_sock.Write(serializedPacket.data(), serializedPacket.size());
                m_clients.pop_back();
                std::cout << "...disconnected client due to chatroom being full\n";
                return;
            }
            
            m_poll.AddMonitor(m_clients.back().m_sock.GetSocket(),
                              ssock::EventType::ReadReady |
                              ssock::EventType::WriteReady);
            m_clients.back().m_sock.SwitchBlockingState();
            if (m_clients.back().m_shouldDisconnect) std::cout << "WILL INSTANTLY DISCONNECT!\n";
        }
    }
    void SleepUntilNextIteration() {
        auto curTime = timer::high_resolution_clock::now();
        auto timeDelta = timer::duration_cast<timer::milliseconds>(curTime - m_lastTime);
        auto sleepTime = timer::milliseconds(SERVER_LOOP_TIME) - timeDelta;
        if (sleepTime > timer::milliseconds(0)) std::this_thread::sleep_for(sleepTime);
    }
};

volatile sig_atomic_t g_isRunning = 1;
void SIGINTCallback(int sig) {g_isRunning = 0;}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, SIGINTCallback);
    int maxClients = 6;
    uint16_t port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (argc > 2) maxClients = std::max(std::min(std::atoi(argv[2]), MAX_CLIENTS), MIN_CLIENTS);
    }
    ssock::WinStartup(); 
    {
        ChatServer server(maxClients);
        if (server.BindAndListen(port) == SOCKET_ERROR) {
            LogError("Failed to start server");
            return static_cast<errcode_t>(ssock::GetLastError());
        }
        while (g_isRunning) {
            server.DisconnectBadClients();
            server.PollSockets();
            server.HandleNetworkData();
            server.AcceptClients();
            server.SleepUntilNextIteration();
        }
    } 
    ssock::WinCleanup();
    std::cout << "Server shutdown\n";
}
