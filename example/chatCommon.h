#pragma once
#include "simpleSock.h"
#include <string>
#include <vector>
#include <chrono>


const size_t MAXIMUM_NAME_LENGTH    = 8,
             MAXIMUM_MESSAGE_LENGTH = 87,
             MAXIMUM_PACKET_SIZE    = 256;

enum class PacketType : uint8_t {
    PACKET_ERROR = 0,
    CONNECT,
    MESSAGE,
    SERVER_STATUS
};

namespace timer = std::chrono;

struct Session {
    std::string m_name;
    bool m_isConnected, m_shouldDisconnect;
    timer::time_point<timer::high_resolution_clock> m_startTime;
    ssock::Socket m_sock;
    Session() = delete;
    Session(ssock::ProtocolType protocol) :
        m_isConnected(false),
        m_shouldDisconnect(false),
        m_sock(protocol),
        m_startTime(timer::high_resolution_clock::now()) {
            m_name.reserve(MAXIMUM_NAME_LENGTH);
        }
    Session(SOCKET acceptedSock) :
        m_isConnected(false),
        m_shouldDisconnect(false),
        m_sock(acceptedSock),
        m_startTime(timer::high_resolution_clock::now()) {
            m_name.reserve(MAXIMUM_NAME_LENGTH);
        }

};

struct Packet {
    uint16_t m_bodySize;
    PacketType m_packetType;
    std::vector<char> m_body;
    Packet() = delete;
    Packet(uint16_t bodySize, PacketType packetType) {
        m_bodySize = bodySize;
        m_packetType = packetType;
        m_body.resize(m_bodySize);
        memset(m_body.data(), 0, m_bodySize);
    }
};

void LogError(const std::string &msg) {
    std::cout << msg << '\n' << "Reason: "
              << ssock::GetErrorMsg(ssock::GetLastError()) << '\n';
}

Packet MakeConnectPacket(std::string name) {
    name = name.substr(0, MAXIMUM_NAME_LENGTH);
    Packet packet(name.size(), PacketType::CONNECT);
    memcpy(packet.m_body.data(), name.data(), name.size());
    return packet;
}

Packet MakeMessagePacket(std::string name, std::string msg) {
    name = name.substr(0, MAXIMUM_NAME_LENGTH);
    msg = msg.substr(0, MAXIMUM_MESSAGE_LENGTH);
    Packet packet(MAXIMUM_NAME_LENGTH + msg.size(), PacketType::MESSAGE);
    memcpy(packet.m_body.data(), name.data(), name.size());
    memcpy(packet.m_body.data() + MAXIMUM_NAME_LENGTH, msg.data(), msg.size());
    return packet;
}
Packet MakeServerStatusPacket(uint16_t clientCount, uint32_t uptime) {
    clientCount = htons(clientCount);
    uptime = htonl(uptime);
    Packet packet(sizeof(uint16_t) + sizeof(uint32_t), PacketType::SERVER_STATUS);
    memcpy(packet.m_body.data(), &clientCount, sizeof(uint16_t));
    memcpy(packet.m_body.data() + sizeof(uint16_t), &uptime, sizeof(uint32_t));
    return packet;
}
void GetNameFromBody(const std::vector<char> &body, std::string &name) {
    size_t nameSize = std::min(body.size(), MAXIMUM_NAME_LENGTH);
    name.resize(nameSize);
    memset(name.data(), 0, nameSize);
    memcpy(name.data(), body.data(), nameSize);
    while (!name.empty() && name.back() == 0) name.pop_back();
}
void GetMessageFromBody(const std::vector<char> &body, std::string &msg) {
    size_t msgSize = (body.size() <= MAXIMUM_NAME_LENGTH) ?
                     0 : body.size() - MAXIMUM_NAME_LENGTH;
    msgSize = std::min(msgSize, MAXIMUM_MESSAGE_LENGTH);
    msg.resize(msgSize);
    memset(msg.data(), 0, msgSize);
    memcpy(msg.data(), body.data() + MAXIMUM_NAME_LENGTH, msgSize);
}
void GetServerStatusFromBody(const std::vector<char> &body, uint16_t &clientCount, uint32_t &uptime) {
    clientCount = 0, uptime = 0;
    if (body.size() < sizeof(uint16_t) + sizeof(uint32_t)) return;
    memcpy(&clientCount, body.data(), sizeof(uint16_t));
    memcpy(&uptime, body.data() + sizeof(uint16_t), sizeof(uint32_t));
    clientCount = ntohs(clientCount);
    uptime = ntohl(uptime);
}
std::vector<char> SerializePacket(const Packet &msg) {
    std::vector<char> packet;
    packet.resize(3+msg.m_body.size());
    size_t offset = 0;
    uint16_t bodySizeConverted = htons(msg.m_bodySize);
    memcpy(packet.data()+offset, &bodySizeConverted, sizeof(msg.m_bodySize));
    offset += sizeof(msg.m_bodySize);
    memcpy(packet.data()+offset, &msg.m_packetType, sizeof(msg.m_packetType));
    offset += sizeof(msg.m_packetType);
    memcpy(packet.data()+offset, msg.m_body.data(), msg.m_body.size());
    return packet;
}
void GetPacketHeader(const std::vector<char> &packet, PacketType &packetType, uint16_t &bodySize) {
    packetType = PacketType::PACKET_ERROR;
    bodySize = 0;
    if (packet.size() < 3) return;
    memcpy(&bodySize, packet.data(), sizeof(uint16_t));
    bodySize = std::min(uint16_t(MAXIMUM_PACKET_SIZE), ntohs(bodySize));
    memcpy(&packetType, packet.data() + sizeof(uint16_t), sizeof(PacketType));
}
void GetPacketBody(const std::vector<char> &packet, uint16_t bodySize, std::vector<char> &body) {
    body.resize(bodySize);
    memset(body.data(), 0, bodySize);
    memcpy(body.data(), 
           packet.data() + sizeof(uint16_t) + sizeof(PacketType),
           bodySize);
}
void DeserializePacket(const std::vector<char> &packet, PacketType& packetType, uint16_t &bodySize, std::vector<char> &body) {
    GetPacketHeader(packet, packetType, bodySize);
    GetPacketBody(packet, bodySize, body);
}
