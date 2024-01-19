#ifndef NFCREADER_H
#define NFCREADER_H

#include <string>
#include <functional>
#include <thread>

extern "C" {
    #include "libpafe/libpafe.h"
}

class NFCReader {
public:
    NFCReader();
    ~NFCReader();

    void startRead(std::function<void(std::string)>);
    void CancelRead();
private:
    pasori* nfc;
    felica* tag;
    bool on_read;
    std::thread th;

    void OpenNFCReader();
    void CloseNFCReader();
    void readNFC(std::function<void(std::string)>);
};

#endif