#pragma once
#include "simpleError.h"
#include "simplePoll.h"
#include <string>
#include <cstring>
#include <iostream>
#include <stdint.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Winsock2.h>
    #include <Ws2tcpip.h>
    typedef int socklen_t;
#else 
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int SOCKET;
    const SOCKET INVALID_SOCKET = ~0;
#endif

#define SSOCK_LOG(msgType, msg) \
    std::cout << msgType << "(nsock::" << __func__ << "): " << msg << '\n';

namespace ssock {
    enum class ProtocolType : uint8_t {TCP, UDP};
    enum class ShutdownType : uint8_t {WRITE, READ, BOTH};
    
    class Socket;
    class Address;
    
    class Socket {
    private:
        bool         m_isBlocking;
        ProtocolType m_protocol;
        SOCKET       m_sock;
    
    public:
        Socket() = delete;
        Socket(ProtocolType);
        Socket(SOCKET);
        Socket(Socket&&) noexcept;
        ~Socket();
        Socket& operator=(Socket&&) noexcept;
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        errcode_t Bind(Address);
        errcode_t Listen(unsigned int);
        SOCKET Accept();
        SOCKET Accept(Address&);
        errcode_t Connect(Address);
    
        errcode_t GetSockAddress(Address&);
        errcode_t GetPeerAddress(Address&);
    
        bool IsBlocking();
        bool IsConnected();
        errcode_t  SwitchBlockingState();
        ProtocolType GetProtocolType();
        SOCKET GetSocket();
    
        int Write(const char*, size_t);
        int Write(const char*, size_t, Address&);
        int Read(char*, size_t);
        int Read(char*, size_t, Address&);
    
        //void      DangerousInvalidateSocket();
        errcode_t Shutdown(ShutdownType);
        errcode_t Close();
    };
    
    class Address {
    private:
        sockaddr_in m_addr;
        socklen_t   m_addrlen;
            
    public:
        Address();
        Address(std::string, uint16_t);
        ~Address();
        bool operator==(const Address& other) const noexcept;
    
        int SetAddress(const std::string&);
        std::string GetAddress() const;
        void SetPort(uint16_t);
        uint16_t GetPort() const;
        std::string GetFullAddress() const;
    
        friend class Socket;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    errcode_t WinStartup() {
        errcode_t ec = 0;
        #ifdef _WIN32
            WSADATA wsaData;
            ec = WSAStartup(MAKEWORD(2, 2), &wsaData);
        //#else 
            //SSOCK_LOG("WARN", "No action under UNIX");
        #endif
        return ec;
    }
    errcode_t WinCleanup() {
        errcode_t ec = 0;
        #ifdef _WIN32
            ec = WSACleanup();
        //#else 
            //SSOCK_LOG("WARN", "No action under UNIX");
        #endif
        return ec;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    Socket::Socket(ProtocolType protocol) {
        m_isBlocking = true;
        m_protocol = protocol;
        switch(protocol) {
            case ProtocolType::TCP:
                m_sock = socket(AF_INET, SOCK_STREAM, 0);
            break;
    
            case ProtocolType::UDP:
                m_sock = socket(AF_INET, SOCK_DGRAM, 0);
            break;
        }
    }
    Socket::Socket(SOCKET acceptedSock) {
        int proto = 0;
        socklen_t protoSize = sizeof(proto);
        m_isBlocking = true;
        m_sock = acceptedSock;
        getsockopt(m_sock, SOL_SOCKET, SO_TYPE, (char*) &proto, &protoSize);
        if (proto == SOCK_STREAM) m_protocol = ProtocolType::TCP;
        else m_protocol = ProtocolType::UDP;
    }
    Socket::Socket(Socket &&moveSock) noexcept {
        m_isBlocking = moveSock.m_isBlocking;
        m_protocol = moveSock.m_protocol;
        m_sock = moveSock.m_sock;
        moveSock.m_sock = INVALID_SOCKET;
    }
    Socket::~Socket() {
        Shutdown(ShutdownType::BOTH);
        Close();
    }
    Socket& Socket::operator=(Socket&& rval) noexcept {
        if (this != &rval) {
            m_isBlocking = rval.m_isBlocking;
            m_protocol = rval.m_protocol;
            m_sock = rval.m_sock;
            rval.m_sock = INVALID_SOCKET;
        }
        return *this;
    }
    
    errcode_t Socket::Bind(Address address) {return bind(m_sock, (sockaddr*) &address.m_addr, address.m_addrlen);}
    errcode_t Socket::Listen(unsigned int backlog) {return listen(m_sock, backlog);}
    SOCKET Socket::Accept() {
        SOCKET acceptedSock = accept(m_sock, nullptr, nullptr);
        return acceptedSock;
    }
    SOCKET Socket::Accept(Address &peerAddress) {
        SOCKET acceptedSock = accept(m_sock, 
                                     (sockaddr*) &peerAddress.m_addr,
                                     &peerAddress.m_addrlen);
        return acceptedSock;
    }
    errcode_t Socket::Connect(Address peerAddress) {return connect(m_sock, (sockaddr*) &peerAddress.m_addr, peerAddress.m_addrlen);}
    
    errcode_t Socket::GetSockAddress(Address& sockAddress) {return getsockname(m_sock, (sockaddr*) &sockAddress.m_addr, &sockAddress.m_addrlen);}
    errcode_t Socket::GetPeerAddress(Address& peerAddress) {return getpeername(m_sock, (sockaddr*) &peerAddress.m_addr, &peerAddress.m_addrlen);}
    bool Socket::IsBlocking() {return m_isBlocking;}
    bool Socket::IsConnected() {
        errcode_t ec;
        socklen_t ecSize = sizeof(ec);
        getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*) &ec, &ecSize);
        return (ec == SUCCESS);
    }
    errcode_t Socket::SwitchBlockingState() {
        errcode_t ec;
        #ifdef _WIN32
            unsigned long mode = (m_isBlocking) ? 1 : 0;
            ec = ioctlsocket(m_sock, FIONBIO, &mode);
        #else 
            int flags = fcntl(m_sock, F_GETFL, 0);
            flags = (m_isBlocking) ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
            ec = fcntl(m_sock, F_SETFL, flags);
        #endif
        if (ec != SOCKET_ERROR) m_isBlocking = !m_isBlocking;
        return ec;
    }
    ProtocolType Socket::GetProtocolType() {return m_protocol;}
    SOCKET Socket::GetSocket() {return m_sock;}
    
    int Socket::Write(const char *buf, size_t bufSize) {
        #ifdef _WIN32
            int sentBytes = send(m_sock, buf, bufSize, 0);
        #else 
            int sentBytes = send(m_sock, buf, bufSize, MSG_NOSIGNAL);
        #endif
        return sentBytes;
    }
    int Socket::Write(const char *buf, size_t bufSize, Address &remoteAddr) {
        int sentBytes;
        if (m_protocol != ProtocolType::UDP) {
            sentBytes = SOCKET_ERROR;
            #ifdef _WIN32 
                WSASetLastError(WSAEOPNOTSUPP);
            #else 
                errno = EOPNOTSUPP;
            #endif
            SSOCK_LOG("SOCKET_ERROR", "Can't send packets to specific remote address in TCP");
        } else {
        #ifdef _WIN32
            sentBytes = sendto(m_sock, buf, bufSize, 
                               0, (sockaddr*) &remoteAddr.m_addr, 
                               remoteAddr.m_addrlen);
        #else 
            sentBytes = sendto(m_sock, buf, bufSize, 
                               MSG_NOSIGNAL, 
                               (sockaddr*) &remoteAddr.m_addr, 
                               remoteAddr.m_addrlen);
        #endif
        }
        return sentBytes;
    }
    int Socket::Read(char *buf, size_t bufSize) {
        int receivedBytes = recv(m_sock, buf, bufSize, 0);
        return receivedBytes;
    }
    int Socket::Read(char *buf, size_t bufSize, Address &remoteAddr) {
        int receivedBytes;
        if (m_protocol != ProtocolType::UDP) {
            receivedBytes = SOCKET_ERROR;
            #ifdef _WIN32 
                WSASetLastError(WSAEOPNOTSUPP);
            #else 
                errno = EOPNOTSUPP;
            #endif
            SSOCK_LOG("ERROR", "Can't receive packets from specific remote address in TCP");
            return receivedBytes;
        }
        receivedBytes = recvfrom(m_sock, buf, bufSize, 
                                 0, (sockaddr*) &remoteAddr.m_addr, 
                                 &remoteAddr.m_addrlen);
        return receivedBytes;
    }

    /*
    // !DANGEROUS! - invalidates underlying file descriptor
    //
    // Might be of use when working with temporary sockets
    //   that you move somewhere else and you want to avoid
    //   destructor closing same fd twice.
    // Obviously, you should not try to call any methods
    //   of invalidated socket
    void Socket::DangerousInvalidateSocket() {m_sock = -1;}
    */
    
    errcode_t Socket::Shutdown(ShutdownType shutdownType) {
        errcode_t ec;
        #ifdef _WIN32
            switch(shutdownType) {
                case ShutdownType::READ:
                    ec = shutdown(m_sock, SD_RECEIVE);
                break;
                case ShutdownType::WRITE:
                    ec = shutdown(m_sock, SD_SEND);
                break;
                case ShutdownType::BOTH:
                    ec = shutdown(m_sock, SD_BOTH);
                break;
            }
        #else 
            switch(shutdownType) {
                case ShutdownType::READ:
                    ec = shutdown(m_sock, SHUT_RD);
                break;
                case ShutdownType::WRITE:
                    ec = shutdown(m_sock, SHUT_WR);
                break;
                case ShutdownType::BOTH:
                    ec = shutdown(m_sock, SHUT_RDWR);
                break;
            }
        #endif
        return ec;
    }
    errcode_t Socket::Close() {
        errcode_t ec;
        #ifdef _WIN32 
            ec = closesocket(m_sock);
        #else 
            ec = close(m_sock);
        #endif
        return ec;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    bool Address::operator==(const Address& other) const noexcept {
        bool addrIsSame = (
                (this->m_addr.sin_family       == other.m_addr.sin_family) &&
                (this->m_addr.sin_port         == other.m_addr.sin_port) &&
                (this->m_addr.sin_addr.s_addr  == other.m_addr.sin_addr.s_addr));
        return (addrIsSame && (this->m_addrlen == other.m_addrlen));
    }

    Address::Address() {
        m_addr.sin_family = AF_INET;
        memset(m_addr.sin_zero, 0, sizeof(m_addr.sin_zero));
        SetAddress("0.0.0.0");
        SetPort(0);
        m_addrlen = sizeof(m_addr);
    }
    Address::Address(std::string addr, uint16_t port) {
        m_addr.sin_family = AF_INET;
        memset(m_addr.sin_zero, 0, sizeof(m_addr.sin_zero));
        SetAddress(addr);
        SetPort(port);
        m_addrlen = sizeof(m_addr);
    }
    Address::~Address() {}
    int Address::SetAddress(const std::string &addr) {return inet_pton(AF_INET, addr.c_str(), &m_addr.sin_addr);}
    std::string Address::GetAddress() const {
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_addr.sin_addr, addr, INET_ADDRSTRLEN);
        return std::string(addr);
    }
    void Address::SetPort(uint16_t port) {m_addr.sin_port = htons(port);}
    uint16_t Address::GetPort() const {return ntohs(m_addr.sin_port);}
    std::string Address::GetFullAddress() const {return GetAddress()+":"+std::to_string(GetPort());}
};
