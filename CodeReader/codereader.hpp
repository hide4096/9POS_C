#ifndef CODEREADER_H
#define CODEREADER_H

#include <string>
#include <functional>
#include <poll.h>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

class CodeReader {
public:
    CodeReader(const std::string& keyboardDevice);
    ~CodeReader();

    void startRead(std::function<void(std::string)>);
    void stopRead();

private:
    int keyboard_fd;
    struct pollfd fds[1];
    const std::string keyboardDevice;
    std::thread th;
    bool on_read;

    void readBarcode(std::function<void(std::string)>);
    int openKeyboardDevice();
    void closeKeyboardDevice();
    char eventkey_to_ascii(int);
};

#endif // CODEREADER_H