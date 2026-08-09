#pragma once
#include "simpleSock.h"
#include "chatCommon.h"
#include <chrono>
#include <deque>
#include <memory>

namespace timer = std::chrono;

const int CONNECT_TIMEOUT = 5000; // in ms

class Client {
private:
    std::unique_ptr<Session> m_session;
    ssock::Poll m_poll;
    std::vector<ssock::pollfde_t> m_readyMonitors;
    std::vector<char> m_buffer;
    std::deque<std::string> m_incomingMessages;
    std::vector<std::string> m_serverStatus;
    std::vector<Packet> m_outcomingPackets;

    void ReadServerData() {
        memset(m_buffer.data(), 0, m_buffer.size());
        int readBytes = m_session->m_sock.Read(m_buffer.data(), m_buffer.size());
        if (readBytes == 0) {
            m_session->m_shouldDisconnect = true;
            return;
        }
        m_session->m_isConnected = true;
        PacketType packetType;
        uint16_t bodySize;
        std::vector<char> body;
        DeserializePacket(m_buffer, packetType, bodySize, body);
        if (packetType == PacketType::MESSAGE) {
            std::string name, msg;
            GetNameFromBody(body, name);
            GetMessageFromBody(body, msg);
            m_incomingMessages.push_front(name+": "+msg);
        } else if (packetType == PacketType::SERVER_STATUS) {
            uint16_t clientCount;
            uint32_t uptime;
            GetServerStatusFromBody(body, clientCount, uptime);
            m_serverStatus.push_back("Server uptime: "+std::to_string(uptime)+" ms");
            m_serverStatus.push_back("Connected users: "+std::to_string(clientCount));
        } else if (readBytes) {m_session->m_shouldDisconnect = true;}
    }
    void WriteServerData() {
        if (m_session->m_shouldDisconnect) return;
        for (const auto& packet : m_outcomingPackets) {
            std::vector<char> serializedPacket = SerializePacket(packet);
            m_session->m_sock.Write(serializedPacket.data(), serializedPacket.size());
        }
        m_outcomingPackets.clear();
    }

public:
    Client() :
        m_buffer(MAXIMUM_PACKET_SIZE, 0)
    {
        m_session.reset();
    }
    ~Client() {}
    bool Disconnect() {
        if (m_session == nullptr) return false;
        m_poll.DeleteMonitor(m_session->m_sock.GetSocket());
        m_session.reset();
        m_readyMonitors.clear();
        m_outcomingPackets.clear();
        m_incomingMessages.clear();
        return true;
    }
    void InitializeConnection(std::string addr, std::string port, std::string name) {
        m_session = std::make_unique<Session>(ssock::ProtocolType::TCP);
        m_session->m_name = name;
        m_session->m_sock.SwitchBlockingState();
        m_poll.AddMonitor(m_session->m_sock.GetSocket(), 
                          ssock::EventType::WriteReady | 
                          ssock::EventType::ReadReady);
        m_session->m_sock.Connect(ssock::Address(addr, uint16_t(std::stoi(port))));
        m_outcomingPackets.push_back(MakeConnectPacket(name));
    }
    void PollSocket() {
        m_readyMonitors.clear();
        ssize_t readyMonitorsCount = m_poll.WaitForReadiness(0);
        if (readyMonitorsCount == SOCKET_ERROR) {
            m_readyMonitors.clear();
            return;
        }
        m_readyMonitors = m_poll.GetReadyMonitors(readyMonitorsCount);
    }
    bool DisconnectBadSession() {
        if (!m_session) return false;
        if (!m_session->m_isConnected) {
            auto curTime = timer::high_resolution_clock::now();
            auto deltaTime = timer::duration_cast<timer::milliseconds>(curTime - m_session->m_startTime);
            if (deltaTime > timer::milliseconds(CONNECT_TIMEOUT)) 
                m_session->m_shouldDisconnect = true;
        }
        if (!m_session->m_shouldDisconnect) return false; 
        return Disconnect();
    }
    void AddMessagePacket(const std::string &msg) {
        if (!m_session) return;
        m_outcomingPackets.push_back(MakeMessagePacket(m_session->m_name, msg));
    }
    std::deque<std::string> MoveIncomingMessages() {
        return std::move(m_incomingMessages);
    }
    std::vector<std::string> MoveServerStatus() {
        return std::move(m_serverStatus);
    }
    void HandleNetworkData() {
        if (m_readyMonitors.empty()) return;
        auto pendingEvent = m_readyMonitors[0].revents;
        if ((pendingEvent & ssock::EventType::ConnectionClosed) ||
            (pendingEvent & ssock::EventType::ErrorOccured) ||
            (pendingEvent & ssock::EventType::InvalidSocket)) {
            m_session->m_shouldDisconnect = true;
        } else {
            if (pendingEvent & ssock::EventType::ReadReady) ReadServerData();
            if (pendingEvent & ssock::EventType::WriteReady) WriteServerData();
        }
    }
};
