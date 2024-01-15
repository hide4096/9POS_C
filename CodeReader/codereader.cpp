#include "codereader.hpp"

CodeReader::CodeReader(const std::string& keyboardDevice)
    : keyboard_fd(-1), keyboardDevice(keyboardDevice)
{
    openKeyboardDevice();
}

CodeReader::~CodeReader()
{
    stopRead();
    closeKeyboardDevice();
}

void CodeReader::startRead(std::function<void(std::string)> callback)
{
    if(th.joinable())
    {
        std::cerr << "Already reading" << std::endl;
        return;
    }

    on_read = true;
    th = std::thread(&CodeReader::readBarcode, this, callback);
}

void CodeReader::stopRead()
{
    if(!th.joinable())
    {
        //std::cerr << "Not reading" << std::endl;
        return;
    }
    on_read = false;
    th.join();
}

void CodeReader::readBarcode(std::function<void(std::string)> callback)
{
    if(keyboard_fd == -1)
    {
        std::cerr << "No CodeReader found" << std::endl;
        return;
    }

    std::string janCode;
    char ch;

    janCode.clear();

    while(on_read)
    {
        poll(fds, 1, 100);
        if(fds[0].revents & POLLIN)
        {
            struct input_event ev;
            read(keyboard_fd, &ev, sizeof(ev));
            if(ev.type == EV_KEY && ev.value != 0)
            {
                if(ev.code == KEY_ENTER)
                {
                    if(janCode.length() == 13 || janCode.length() == 8)
                    {
                        callback(janCode);
                    }
                    janCode.clear();
                }
                else
                {
                    ch = eventkey_to_ascii(ev.code);
                    //入力は0~9かA~Zのみ
                    if((ch >= 0x30 && ch <= 0x39) || (ch >= 0x41 && ch <= 0x5a))
                    {
                        janCode += ch;
                    }
                }
            }

        }
    }
}

int CodeReader::openKeyboardDevice()
{
    keyboard_fd = open(keyboardDevice.c_str(), O_RDONLY);
    ioctl(keyboard_fd, EVIOCGRAB, 1);   //権限を獲得
    if (keyboard_fd == -1)
    {
        perror("open");
        return -1;
    }

    fds[0].fd = keyboard_fd;
    fds[0].events = POLLIN | POLLERR;

    return 0;
}

void CodeReader::closeKeyboardDevice()
{
    if (keyboard_fd != -1)
    {
        ioctl(keyboard_fd, EVIOCGRAB, 0);   //権限を解放
        close(keyboard_fd);
    }
}

char CodeReader::eventkey_to_ascii(int eventkey)
{
    switch(eventkey)
    {
        case KEY_0:
            return '0';
        case KEY_1:
            return '1';
        case KEY_2:
            return '2';
        case KEY_3:
            return '3';
        case KEY_4:
            return '4';
        case KEY_5:
            return '5';
        case KEY_6:
            return '6';
        case KEY_7:
            return '7';
        case KEY_8:
            return '8';
        case KEY_9:
            return '9';
        case KEY_A:
            return 'A';
        case KEY_B:
            return 'B';
        case KEY_C:
            return 'C';
        case KEY_D:
            return 'D';
        case KEY_E:
            return 'E';
        case KEY_F:
            return 'F';
        case KEY_G:
            return 'G';
        case KEY_H:
            return 'H';
        case KEY_I:
            return 'I';
        case KEY_J:
            return 'J';
        case KEY_K:
            return 'K';
        case KEY_L:
            return 'L';
        case KEY_M:
            return 'M';
        case KEY_N:
            return 'N';
        case KEY_O:
            return 'O';
        case KEY_P:
            return 'P';
        case KEY_Q:
            return 'Q';
        case KEY_R:
            return 'R';
        case KEY_S:
            return 'S';
        case KEY_T:
            return 'T';
        case KEY_U:
            return 'U';
        case KEY_V:
            return 'V';
        case KEY_W:
            return 'W';
        case KEY_X:
            return 'X';
        case KEY_Y:
            return 'Y';
        case KEY_Z:
            return 'Z';
        default:
            return '\0';
    }
}