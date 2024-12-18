#ifndef RUDP_H // 防止头文件冲突，如果已经存在就跳过这次的定义
#define RUDP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>

// 防止头文件冲突的宏定义
#pragma comment(lib, "Ws2_32.lib")

// 常量定义
const int MAX_BUFFER_SIZE = 1024;
const int HEADER_SIZE = 12; // type (4 bytes) + seq (4 bytes) + checksum (4 bytes)
const int DATA_SIZE = MAX_BUFFER_SIZE - HEADER_SIZE;

// Message Types 消息类型的枚举定义
enum MessageType {
    SYN = 1,
    SYN_ACK,
    ACK,
    DATA,
    DATA_ACK,
    FIN,
    FIN_ACK
};

// Packet Structure 报文的数据结构定义，接收到报文后可以直接通过指针转换成这个类型，直接读取对应的内存段
struct Packet {
    uint32_t type;
    uint32_t seq;
    uint32_t checksum;
    char data[DATA_SIZE];

    Packet() : type(0), seq(0), checksum(0) {
        memset(data, 0, DATA_SIZE);
    }
};

// Function to calculate checksum 计算校验值
uint32_t calculateChecksum(Packet& pkt) {
    uint32_t sum = pkt.type + pkt.seq;
    for (int i = 0; i < DATA_SIZE; ++i) {
        sum += static_cast<uint8_t>(pkt.data[i]);
    }
    return sum;
}

// Function to send a packet 发送一个数据报
ssize_t sendPacket(SOCKET sockfd, const Packet& pkt, const sockaddr_in& addr) {
    Packet send_pkt = pkt;
    send_pkt.checksum = calculateChecksum(send_pkt);
    int bytes_sent = sendto(sockfd, reinterpret_cast<const char*>(&send_pkt), sizeof(send_pkt), 0,
                            reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }
    return bytes_sent;
}

// Function to receive a packet with timeout 接收一个数据报，可以设定超时时间
ssize_t recvPacket(SOCKET sockfd, Packet& pkt, sockaddr_in& addr, int timeout_sec = 1) {
    socklen_t addr_len = sizeof(addr);

    // 设置超时时间
    DWORD timeout = timeout_sec * 1000; // 转换为毫秒
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    int bytes_received = recvfrom(sockfd, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0,
                                  reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (bytes_received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
            // 超时
            return 0;
        } else {
            std::cerr << "recvfrom failed with error: " << error << std::endl;
            return -1;
        }
    }

    uint32_t received_checksum = pkt.checksum;
    pkt.checksum = 0;
    uint32_t calculated_checksum = calculateChecksum(pkt);
    if (received_checksum != calculated_checksum) {
        std::cerr << "Checksum mismatch!" << std::endl;
        return -1; // Indicate checksum error 校验不通过
    }
    return bytes_received;
}

#endif // RUDP_H
