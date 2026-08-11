#include "simpleSock.h"
#include "chatClientNetworkLayer.h"
#include <csignal>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <deque>

#ifdef _WIN32
    #include <ncurses\ncurses.h>
#else
    #include <ncurses.h>
#endif

namespace timer = std::chrono;
const std::vector<std::string> COMMANDS_LIST = {
    "/exit - quit program",
    "/disconnect - disconnect",
    "/connect <IP> <PORT> <NAME> - connect",
    "/clear - clears output history"
};
const int TARGET_LOOP_TIME    = 20, // in ms
          INPUT_BUFFER_SIZE   = 87,
          OUTPUT_HISTORY_SIZE = 18,
                                   
          OUTPUT_WIN_HEIGHT = 20,
          OUTPUT_WIN_WIDTH  = 100,
          OUTPUT_WIN_X      = 2,
          OUTPUT_WIN_Y      = 2,

          INPUT_WIN_HEIGHT  = 3,
          INPUT_WIN_WIDTH   = OUTPUT_WIN_WIDTH,
          INPUT_WIN_X       = OUTPUT_WIN_X,
          INPUT_WIN_Y       = OUTPUT_WIN_Y + OUTPUT_WIN_HEIGHT,

          STATUS_WIN_HEIGHT  = OUTPUT_WIN_HEIGHT + INPUT_WIN_HEIGHT,
          STATUS_WIN_WIDTH   = 45,
          STATUS_WIN_X       = INPUT_WIN_X + INPUT_WIN_WIDTH,
          STATUS_WIN_Y       = OUTPUT_WIN_Y;

std::vector<std::string> SplitString(std::string str) {
    std::vector<std::string> res;
    std::string temp;
    for (size_t i = 0; i != str.size(); ++i) {
        if (str[i] == ' ') {
            while (i != str.size() && str[i] == ' ') ++i;
            if (i == str.size()) break;
            --i;
            if (!temp.empty()) {
                res.push_back(temp);
                temp.clear();
            }
        } else temp.push_back(str[i]);
    }
    if (!temp.empty()) res.push_back(temp);
    return res;
}
bool StringIsNumber(const std::string &str) {
    for (const auto c : str)
        if (!std::isdigit(c)) return false;
    return true;
}

void OutputVector(WINDOW *win, const std::vector<std::string> &vec, int Y, int X) {
    for (const auto &word : vec) {
        mvwaddstr(win, Y, X, word.c_str());
        ++Y;
    }
}
void OutputVector(WINDOW *win, const std::deque<std::string> &deq, int Y, int X) {
    for (const auto &word : deq) {
        mvwaddstr(win, Y, X, word.c_str());
        ++Y;
    }
}

struct WindowWrapper {
    WINDOW *win;
    WindowWrapper() = delete;
    WindowWrapper(int height, int width, int y, int x) {
        win = newwin(height, width, y, x);
    }
    ~WindowWrapper() {
        wclear(win);
        wrefresh(win);
        delwin(win);
    }
};

class ChatClientApp {
private:
    bool m_isRunning,
         m_outputUpdated, m_inputUpdated, m_statusUpdated;
    WindowWrapper m_output,
                  m_input,
                  m_status;
    timer::time_point<timer::high_resolution_clock> m_lastTime;
    Client m_client;
    std::string m_userInputBuffer;
    std::vector<std::string> m_serverStatus;
    std::deque<std::string>  m_outputHistory;

    void UpdateOutputHistory(const std::string &msg) {
        if (msg.empty()) return;
        if (m_outputHistory.size() >= OUTPUT_HISTORY_SIZE) m_outputHistory.pop_back();
        m_outputHistory.push_front(msg);
        m_outputUpdated = true;
    }
    void CheckInputForValidMessages() {
        if (m_userInputBuffer.front() == '/') {
            if (m_userInputBuffer.substr(0, 5) == "/exit") {
                m_isRunning = false;
                UpdateOutputHistory("SYSTEM: Shutting down");
            }
            else if (m_userInputBuffer.substr(0,6) == "/clear") {
                m_outputHistory.clear();
                m_outputUpdated = true;
            }
            else if (m_userInputBuffer.substr(0,11) == "/disconnect") {
                if (m_client.Disconnect()) {
                    m_serverStatus.clear();
                    m_statusUpdated = true;
                    UpdateOutputHistory("SYSTEM: Disconnected from server");
                }
            }
            else if (m_userInputBuffer.substr(0,8)  == "/connect") {
                std::vector<std::string> split = SplitString(m_userInputBuffer);
                if (split.size() >= 4 && StringIsNumber(split[2])) {
                    if (m_client.Disconnect()) {
                        m_serverStatus.clear();
                        m_statusUpdated = true;
                        UpdateOutputHistory("SYSTEM: Disconnected from server");
                    }
                    split[3] = split[3].substr(0, MAXIMUM_NAME_LENGTH);
                    m_client.InitializeConnection(split[1], split[2], split[3]);
                    UpdateOutputHistory("SYSTEM: Connecting to: "
                                        + split[1] + ":" + split[2] 
                                        + " | name = " + split[3]);
                }
            }
        } else m_client.AddMessagePacket(m_userInputBuffer);
    }

public:
    ChatClientApp() :
        m_output(OUTPUT_WIN_HEIGHT, OUTPUT_WIN_WIDTH,
                 OUTPUT_WIN_Y, OUTPUT_WIN_X),
        m_input(INPUT_WIN_HEIGHT, INPUT_WIN_WIDTH,
                INPUT_WIN_Y, INPUT_WIN_X),
        m_status(STATUS_WIN_HEIGHT, STATUS_WIN_WIDTH,
                 STATUS_WIN_Y, STATUS_WIN_X)
    {
        m_userInputBuffer.reserve(INPUT_BUFFER_SIZE);
        m_outputUpdated = true;
        m_inputUpdated = true;
        m_statusUpdated = true;
        m_isRunning = true;
        initscr();
        cbreak();
        nodelay(m_input.win, true);
        keypad(m_input.win, true);
        noecho();
    }
    ~ChatClientApp() {}

    bool IsRunning() {return m_isRunning;}
    void StartIteration() {m_lastTime = timer::high_resolution_clock::now();}
    void SleepUntilNextIteration() {
        auto endTime = timer::high_resolution_clock::now();
        auto sleepTime = timer::milliseconds(TARGET_LOOP_TIME) -
                         timer::duration_cast<timer::milliseconds>(endTime - m_lastTime);
        if (sleepTime > timer::milliseconds(0)) std::this_thread::sleep_for(sleepTime);
    }
    void GetUserInput() {
        int ch = wgetch(m_input.win);  
        while (ch != ERR) {
            m_inputUpdated = true;
            if ((ch == '\n' || ch == '\r') && !m_userInputBuffer.empty()) {
                CheckInputForValidMessages();
                m_userInputBuffer.clear();
            } else if (ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127) {
                if (!m_userInputBuffer.empty()) 
                    m_userInputBuffer.pop_back();
            } else if (ch >= 32 && ch < 127)
                if (m_userInputBuffer.size() < INPUT_BUFFER_SIZE-1) 
                    m_userInputBuffer.push_back(ch);
            ch = wgetch(m_input.win);
        }
    }
    void HandleNetwork() {
        m_client.PollSocket();
        if (m_client.DisconnectBadSession()) {
            m_serverStatus.clear();
            m_statusUpdated = true;
            UpdateOutputHistory("SYSTEM: Disconnected from server");
        }
        m_client.HandleNetworkData();
        std::deque<std::string> incomingMessages(m_client.MoveIncomingMessages());
        while (!incomingMessages.empty()) {
            UpdateOutputHistory(incomingMessages.front());
            incomingMessages.pop_front();
        }
        m_serverStatus = std::move(m_client.MoveServerStatus());
        if (!m_serverStatus.empty()) m_statusUpdated = true;
    }
    void DrawOutputWindow() {
        if (!m_outputUpdated) return;
        wclear(m_output.win);
        box(m_output.win, 0, 0);
        OutputVector(m_output.win, m_outputHistory, 1, 2);
        wrefresh(m_output.win);
        m_outputUpdated = false;
    }
    void DrawInputWindow() {
        if (!m_inputUpdated) return;
        wclear(m_input.win);
        box(m_input.win, 0, 0);
        mvwaddstr(m_input.win, 1, 2, m_userInputBuffer.c_str());
        wrefresh(m_input.win);
        m_inputUpdated = false;
    }
    void DrawStatusWindow() {
        if (!m_statusUpdated) return;
        int Y = 1, X = 2;
        wclear(m_status.win);
        box(m_status.win, 0, 0);
        OutputVector(m_status.win, COMMANDS_LIST, Y, X);
        Y += COMMANDS_LIST.size() + 2;
        OutputVector(m_status.win, m_serverStatus, Y, X);
        wrefresh(m_status.win);
        m_statusUpdated = false;
    }
};

volatile sig_atomic_t g_isRunning = 1;
void SIGINTCallback(int sig) {g_isRunning = 0;}

int main() {
    std::signal(SIGINT, SIGINTCallback);
    ssock::WinStartup();
    initscr();
    {
        ChatClientApp app;
        while (app.IsRunning() && g_isRunning) {
            app.StartIteration();
            app.GetUserInput();
            app.HandleNetwork();
            app.DrawOutputWindow();
            app.DrawInputWindow();
            app.DrawStatusWindow();
            app.SleepUntilNextIteration();
        }
    }
    endwin();
    ssock::WinCleanup();
}
