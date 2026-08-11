#pragma once
#include "simpleError.h"
#include <vector>
#include <array>
#include <unordered_map>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Winsock2.h>
#else 
    #include <poll.h>
    typedef int SOCKET;
#endif

const int MONITOR_DEFAULT_SIZE = 32;
const ssize_t MONITOR_DOES_NOT_EXIST = -1;
typedef short EVENT;

/*
EVENTS
WINDOWS     LINUX            
POLLPRI 	POLLRDBAND    Not supported win/unix
POLLRDBAND 	POLLPRI       Priority band (out-of-band) data can be read without blocking.
POLLRDNORM 	POLLIN        Normal data can be read without blocking.
POLLWRNORM 	POLLOUT       Normal data can be written without blocking.

REVENTS
WINDOWS     LINUX         
POLLERR 	POLLERR       An error has occurred.
POLLHUP 	POLLHUP       A stream-oriented connection was either disconnected or aborted.
POLLNVAL 	POLLNVAL      An invalid socket was used.
*/

namespace ssock {
    enum class EventType : EVENT {
        ReadReady        = 0b0000000000000001,
        OutOfBandReady   = 0b0000000000000010,
        WriteReady       = 0b0000000000000100,
        ErrorOccured     = 0b0000000000001000,
        ConnectionClosed = 0b0000000000010000,
        InvalidSocket    = 0b0000000000100000
    };

    constexpr EventType operator|(EventType lhs, EventType rhs) {
        return static_cast<EventType>(
                static_cast<EVENT>(lhs) | static_cast<EVENT>(rhs));
    }

    constexpr bool operator&(EventType lhs, EventType rhs) {
        return bool(static_cast<EVENT>(lhs) & static_cast<EVENT>(rhs));
    }

    struct pollfde_t {
        SOCKET fd;          /* socket file descriptor*/
        EventType events;   /* requested events */
        EventType revents;  /* returned events */
    };

    const std::array<EventType, 6> EventTypeArray = {
        EventType::ReadReady, EventType::OutOfBandReady,
        EventType::WriteReady, EventType::ErrorOccured,
        EventType::ConnectionClosed, EventType::InvalidSocket};

    #ifdef _WIN32 
    const std::array<EVENT, 6> NativeEventArray = {
            POLLRDNORM, POLLRDBAND, POLLWRNORM, 
            POLLERR, POLLHUP, POLLNVAL};

        const std::unordered_map<EventType, EVENT> eventTypeToNativeEvent = {
            {EventType::ReadReady,         POLLRDNORM},
            {EventType::OutOfBandReady,    POLLRDBAND},
            {EventType::WriteReady,        POLLWRNORM},
            {EventType::ErrorOccured,      POLLERR},
            {EventType::ConnectionClosed,  POLLHUP},
            {EventType::InvalidSocket,     POLLNVAL}
        };
        const std::unordered_map<EVENT, EventType> nativeEventToEventType = {
            {POLLRDNORM,      EventType::ReadReady},
            {POLLRDBAND,      EventType::OutOfBandReady},
            {POLLWRNORM,      EventType::WriteReady,},
            {POLLERR,         EventType::ErrorOccured},
            {POLLHUP,         EventType::ConnectionClosed},
            {POLLNVAL,        EventType::InvalidSocket}
        };
    #else 
        const std::array<EVENT, 6> NativeEventArray = {
            POLLIN, POLLPRI, POLLOUT, 
            POLLERR, POLLHUP, POLLNVAL};

        const std::unordered_map<EventType, EVENT> eventTypeToNativeEvent = {
            {EventType::ReadReady,         POLLIN},
            {EventType::OutOfBandReady,    POLLPRI},
            {EventType::WriteReady,        POLLOUT},
            {EventType::ErrorOccured,      POLLERR},
            {EventType::ConnectionClosed,  POLLHUP},
            {EventType::InvalidSocket,     POLLNVAL}
        };
        const std::unordered_map<EVENT, EventType> nativeEventToEventType = {
            {POLLIN,          EventType::ReadReady},
            {POLLPRI,         EventType::OutOfBandReady},
            {POLLOUT,         EventType::WriteReady,},
            {POLLERR,         EventType::ErrorOccured},
            {POLLHUP,         EventType::ConnectionClosed},
            {POLLNVAL,        EventType::InvalidSocket}
        };
    #endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class Poll {
    private:
        std::vector<pollfd> m_monitoredSockets;

        ssize_t   MonitorExists(SOCKET);
        EVENT     GetNativeEventType(EventType);
        EventType GetEventType(EVENT);

    public:
        Poll();
        ~Poll();
        errcode_t AddMonitor(SOCKET, EventType);
        errcode_t ModifyMonitor(SOCKET, EventType);
        errcode_t DeleteMonitor(SOCKET);
        ssize_t   WaitForReadiness(int);
        std::vector<pollfde_t> GetReadyMonitors(size_t);
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ssize_t Poll::MonitorExists(SOCKET sockfd) {
        size_t monitorId = 0;
        for (; monitorId < m_monitoredSockets.size(); ++monitorId) 
            if (m_monitoredSockets[monitorId].fd == sockfd) return ssize_t(monitorId);
        return MONITOR_DOES_NOT_EXIST;
    }
    EVENT Poll::GetNativeEventType(EventType event) {
        EVENT nativeEvent = 0;
        for (const auto eventType : EventTypeArray)
            if (event & eventType) {
                auto it = eventTypeToNativeEvent.find(eventType);
                if (it != eventTypeToNativeEvent.end()) nativeEvent |= it->second;
            }
        return nativeEvent;
    }
    EventType Poll::GetEventType(EVENT event) {
        EventType eventType = static_cast<EventType>(0);
        for (const auto nativeEvent : NativeEventArray)
            if (event & nativeEvent) {
                auto it = nativeEventToEventType.find(nativeEvent);
                if (it != nativeEventToEventType.end()) 
                    eventType = eventType | it->second;
            }
        return eventType;
    }

    Poll::Poll() {m_monitoredSockets.reserve(MONITOR_DEFAULT_SIZE);}
    Poll::~Poll() {}
    errcode_t Poll::AddMonitor(SOCKET sockfd, EventType events) {
        errcode_t ec = SUCCESS;
        pollfd monitor = {sockfd, 0, 0};
        if (MonitorExists(sockfd) != MONITOR_DOES_NOT_EXIST) {
            ec = SOCKET_ERROR;
            #ifdef _WIN32 
                WSASetLastError(WSAEBADF);
            #else 
                errno = EBADF;
            #endif
            return ec;
        }
        monitor.events = GetNativeEventType(events);
        m_monitoredSockets.push_back(monitor);
        return ec;
    }
    errcode_t Poll::ModifyMonitor(SOCKET sockfd, EventType events) {
        ssize_t monitorId = MonitorExists(sockfd);
        errcode_t ec = SUCCESS;
        if (monitorId == MONITOR_DOES_NOT_EXIST) {
            ec = SOCKET_ERROR;
            #ifdef _WIN32 
                WSASetLastError(WSAEBADF);
            #else 
                errno = EBADF;
            #endif
            return ec;
        }
        m_monitoredSockets[monitorId].events = GetNativeEventType(events);
        return ec;
    }
    errcode_t Poll::DeleteMonitor(SOCKET sockfd) {
        errcode_t ec = SUCCESS;
        ssize_t monitorId = MonitorExists(sockfd);
        if (monitorId == MONITOR_DOES_NOT_EXIST) {
            ec = SOCKET_ERROR;
            #ifdef _WIN32 
                WSASetLastError(WSAEBADF);
            #else 
                errno = EBADF;
            #endif
            return ec;
        }
        std::swap(m_monitoredSockets[monitorId], 
                  m_monitoredSockets.back());
        m_monitoredSockets.pop_back();
        return ec;
    }
    ssize_t Poll::WaitForReadiness(int timeout) {
        #ifdef _WIN32
            return WSAPoll(m_monitoredSockets.data(), m_monitoredSockets.size(), timeout);
        #else 
            return poll(m_monitoredSockets.data(), m_monitoredSockets.size(), timeout);
        #endif
    }
    std::vector<pollfde_t> Poll::GetReadyMonitors(size_t monitorAmount) {
        std::vector<pollfde_t> readyMonitors;
        pollfde_t temp;
        readyMonitors.reserve(monitorAmount);
        for (size_t id = 0; id != m_monitoredSockets.size() && monitorAmount; ++id) {
            if (m_monitoredSockets[id].revents != 0) {
                temp.fd = m_monitoredSockets[id].fd;
                temp.events = GetEventType(m_monitoredSockets[id].events);
                temp.revents = GetEventType(m_monitoredSockets[id].revents);
                readyMonitors.push_back(temp);
                --monitorAmount;
            }
        }
        return readyMonitors;
    }
};
