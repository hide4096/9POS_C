#include "NFCReader/nfcreader.hpp"
#include <iostream>

NFCReader::NFCReader(){
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

NFCReader::~NFCReader(){
    CloseNFCReader();
}

void NFCReader::CloseNFCReader(){
    if(nfc != nullptr){
        pasori_close(nfc);
        nfc = nullptr;
    }
}

void NFCReader::startRead(std::function<void(std::string)> callback){
    if(th.joinable()){
        std::cerr << "Already reading" << std::endl;
        return;
    }

    on_read = true;
    th = std::thread(&NFCReader::readNFC, this, callback);
}

void NFCReader::CancelRead(){
    if(!th.joinable()){
        std::cerr << "Not reading" << std::endl;
        return;
    }
    on_read = false;
    th.join();
}

void NFCReader::readNFC(std::function<void(std::string)> callback){
    if(nfc == nullptr){
        std::cerr << "No NFCReader found" << std::endl;
        return;
    }

    felica* tag;
    while(on_read){
        tag = felica_polling(nfc, FELICA_POLLING_ANY, 0, 0);
        if(tag != nullptr){
            break;
        }
    }

    uint8_t idm;
    if(felica_get_idm(tag, &idm) != 0){
        std::cerr << "NFCReader get idm failed" << std::endl;
        return;
    }

    free(nfc);
    callback(std::to_string(idm));
}
