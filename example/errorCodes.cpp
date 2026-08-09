#include "simpleSock.h"

int main() {
    ssock::WinStartup();
    {
        std::cout << "Native error message (0) = " << ssock::GetNativeErrorMsg(0) << '\n';
        std::cout << "Native error message (-1) = " << ssock::GetNativeErrorMsg(-1) << '\n';

        ssock::Socket sock(ssock::ProtocolType::TCP);
        ssock::Address malformedAddr("999.999.999.999", 22);
        errcode_t res = sock.Bind(malformedAddr);
        
        if (res == SOCKET_ERROR) {
            std::cout << "Failed to bind socket!\n";
            errcode_t ec = ssock::GetLastNativeError();
            ssock::SocketError se = ssock::GetLastError();

            std::cout << "Native errcode = " << ec << '\n';
            std::cout << "Native error message = " << ssock::GetNativeErrorMsg(ec) << '\n';

            std::cout << "OS-agnostic errcode = " << static_cast<errcode_t>(se) << '\n';
            std::cout << "OS-agnostic error message = " << ssock::GetErrorMsg(se) << '\n';
        }
    }
    ssock::WinCleanup();
}
