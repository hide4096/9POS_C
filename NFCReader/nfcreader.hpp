#ifndef NFCREADER_H
#define NFCREADER_H

#include <string>
#include <functional>
#include "libpafe/libpafe.h"
#include <thread>

class NFCReader {
public:
    NFCReader();
    ~NFCReader();

    void startRead(std::function<void(std::string)>);
    void CancelRead();
private:
    pasori* nfc;
    bool on_read;
    std::thread th;

    void CloseNFCReader();
    void readNFC(std::function<void(std::string)>);
};

#endif