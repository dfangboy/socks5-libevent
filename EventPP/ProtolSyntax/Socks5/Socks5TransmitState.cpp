//
//  Socks5TransmitState.cpp
//  EventPP
//
//  Created by CmST0us on 2017/9/11.
//  Copyright © 2017年 CmST0us. All rights reserved.
//

#include <iostream>

#include "TCPConnectionFactory.hpp"
#include "TCPConnection.hpp"
#include "CustomSyntaxAdapter.hpp"

#include "Socks5TransmitState.hpp"
#include "SocketException.hpp"
#include "TCPConnection.hpp"
#include "TCPServer.hpp"

using namespace ts;

void Socks5TransmitState::handle(Socks5State::SOCKS5_STATE state) {
    switch (state) {
        case Socks5State::STATE_AUTH: {
            //不支持
            throw SocketException::socks5UnsupportStateError;
        }
            break;
        case Socks5State::STATE_TRANSMIT: {
            //不支持
            throw SocketException::socks5UnsupportStateError;
        }
            break;
        case Socks5State::STATE_LINK_REQUEST: {
            throw SocketException::socks5UnsupportStateError;
        }
            break;
        default:
            throw SocketException::socks5UnsupportStateError;
            break;
    }
}

void Socks5TransmitState::createRemoteConnection(TCPConnection *ctx) {
    this->mLocalConnection = ctx;
    std::cout<<"target: "<<this->mRemoteHost<<":"<<this->mRemotePort<<std::endl;
    
    this->mRemoteConnection = TCPConnectionFactory::longLinkTCPConnection();
    auto customSyntax = (CustomSyntaxAdapter *)mRemoteConnection->getProtocolSyntax();
    
    customSyntax->mOnEOFEventHandler = [&](void *ctx) {
//        remote - > P -> local eof
        this->mLocalConnection->shutdown(SHUT_RD);
        this->mRemoteConnection->shutdown(SHUT_WR);
    };
    
    customSyntax->mOnSignalEventHandler = [&](int fd, short what, void *arg) {
        printf("remote signal\n");
        this->mRemoteConnection->close();
        this->mRemoteConnection->stop();
        this->mLocalConnection->close();
        this->mLocalConnection->stop();
        auto localContext = (TCPServer *)this->mLocalConnection->getContext();
        localContext->removeConnectionWithKey(this->mClientKey);
    };
    
    customSyntax->mStreamHandler = [&](ProtocolSyntax::EventType type, InputStream* input, OutputStream *output, void *ctx) {
        //   (触发)
        // R -> P -> L
        auto outputStream = this->mLocalConnection->getOutputStream();
        auto inputStream = this->mLocalConnection->getInputStream();
        
        int inputLen = (int)input->length();
        if (inputLen > 0) {
            unsigned char *inputBuffer = new unsigned char[inputLen];
            memset(inputBuffer, 0, inputLen);
            
            input->read(inputBuffer, inputLen);
            outputStream->write(inputBuffer, inputLen);
            delete [] inputBuffer;
        }
    };
    
    try {
        mRemoteConnection->connect(this->mRemoteHost, this->mRemotePort);
    } catch (SocketException e) {
        std::cout<<"connect remote error:"<<e.getExceptionDescription()<<std::endl;
        this->mLocalConnection->close();
    }
    
    std::thread([&](){
        mRemoteConnection->start();
        delete mRemoteConnection;
        this->mRemoteConnection = nullptr;
    }).detach();
}

void Socks5TransmitState::onSignalEvent(int fd, short what, void *arg) {
//    if (this->mLocalConnection != nullptr) {
//        this->mLocalConnection->close();
//        this->mLocalConnection->stop();
//    }
//    if (this->mRemoteConnection != nullptr) {
//        this->mRemoteConnection->close();
//        this->mRemoteConnection->stop();
//    }
}

//local - > P -> remote eof
void Socks5TransmitState::onEOFEvent(void *ctx) {
    this->mRemoteConnection->shutdown(SHUT_WR);
    this->mLocalConnection->shutdown(SHUT_RD);
}

void Socks5TransmitState::feed(ProtocolSyntax::EventType type, InputStream *inputStream, OutputStream *outputStream, void *ctx) {
    
    auto localToProxyConnection = (TCPConnection *)ctx;
    auto key = localToProxyConnection->getSocketAddress().ipPortPairString();
    
    if (key.compare(this->mClientKey) != 0) {
        return;
    }
    //   (触发)
    // L -> P -> R
    int rdl = (int)inputStream->length();
    if (rdl > 0) {
        unsigned char *rdbuf = new unsigned char[rdl];
        memset(rdbuf, 0, rdl);
        inputStream->read(rdbuf, rdl);
        
        this->mRemoteConnection->getOutputStream()->write(rdbuf, rdl);
        delete [] rdbuf;
    }
    
}
