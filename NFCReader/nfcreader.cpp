#include "NFCReader/nfcreader.hpp"
#include <iostream>
#include <sstream>

NFCReader::NFCReader(){}

NFCReader::~NFCReader(){
    CancelRead();
}

void NFCReader::OpenNFCReader(){
    if(nfc == nullptr){
        nfc = pasori_open();
        if(nfc == nullptr){
            std::cerr << "NFCReader not found" << std::endl;
            return;
        }
        if(pasori_init(nfc) != 0){
            std::cerr << "NFCReader init failed" << std::endl;
            return;
        }
        pasori_set_timeout(nfc, 300);
    }
}

void NFCReader::CloseNFCReader(){
    if(nfc != nullptr){
        pasori_close(nfc);
        nfc = nullptr;
    }
}

void NFCReader::startRead(std::function<void(std::string)> callback){
    if(th.joinable()){
        std::cerr << "NFCReader is already running" << std::endl;
        return;
    }

    OpenNFCReader();

    on_read = true;
    th = std::thread(&NFCReader::readNFC, this, callback);
}

void NFCReader::CancelRead(){
    if(!th.joinable()){
        //std::cerr << "Not reading" << std::endl;
        return;
    }

    CloseNFCReader();

    on_read = false;
    th.join();
}

void NFCReader::readNFC(std::function<void(std::string)> callback){
    while(on_read){
        if(nfc == nullptr){
            std::cerr << "NFCReader is not initialized" << std::endl;
            return;
        }

        tag = nullptr;
        while(tag == nullptr){
            tag = felica_polling(nfc, FELICA_POLLING_ANY, 0, 0);
            if(!on_read){
                return;
            }
        }

        uint64_t idm;
        if(felica_get_idm(tag, (uint8*)&idm) != 0){
            std::cerr << "NFCReader get idm failed" << std::endl;
            return;
        }

        free(tag);
        //16進数で送る
        std::stringstream ss;
        ss << std::hex << std::uppercase << idm;
        callback(ss.str());
    }
}
