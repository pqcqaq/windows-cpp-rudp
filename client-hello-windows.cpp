#include <rudp.h>
#include <cstring> // 为 strncpy 引入头文件
#include <iostream>
#include <winsock2.h> // Windows 套接字 API
#include <ws2tcpip.h> // 用于处理 IP 地址和主机名解析

// 这里是客户端的实现
// 先发文件，再收文件
// 三次握手和四次挥手
int main(int argc, char* argv[]) {
    // 检查参数
    if (argc != 2) {
        std::cerr << "Usage: ./client <host>:<port>" << std::endl;
        return -1;
    }

    std::string host_port = argv[1];

    size_t colon_pos = host_port.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid host:port format" << std::endl;
        return -1;
    }

    std::string host = host_port.substr(0, colon_pos);
    int port = atoi(host_port.substr(colon_pos + 1).c_str());

    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    SOCKET sockfd;
    sockaddr_in server_addr{};

    // 创建 UDP 套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // 使用 WSAStringToAddress 来转换 IP 地址字符串到 sockaddr_in 结构
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;

    // 定义地址长度变量
    int sa_len = sizeof(sa);
    
    int result = WSAStringToAddressA(const_cast<char*>(host.c_str()), AF_INET, NULL, (struct sockaddr*)&sa, &sa_len);
    if (result != 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    server_addr.sin_addr = sa.sin_addr; // 复制 IP 地址

    // 连接建立（三次握手）
    Packet pkt;
    Packet recv_pkt;

    // 发送 SYN
    pkt.type = SYN;
    pkt.seq = 0;
    sendPacket(sockfd, pkt, server_addr);
    std::cout << "Sent SYN to server" << std::endl;

    // 等待 SYN-ACK
    while (true) {
        ssize_t n = recvPacket(sockfd, recv_pkt, server_addr);
        if (n > 0 && recv_pkt.type == SYN_ACK) {
            std::cout << "Received SYN-ACK from server" << std::endl;
            // 发送 ACK
            pkt.type = ACK;
            pkt.seq = recv_pkt.seq;
            sendPacket(sockfd, pkt, server_addr);
            std::cout << "Sent ACK to server" << std::endl;
            break; // 连接建立
        }
    }

    // 给服务器发信息
    uint32_t seq_num = 0;
    while (true) {
        Packet data_pkt;
        data_pkt.type = DATA;
        data_pkt.seq = seq_num;
        // 原始字符数组
        char char_array[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '\0'}; // 注意结尾加 \0 以标识字符串结束
        // 使用 strncpy 将字符串复制到固定大小的 char 数组中
        std::strncpy(data_pkt.data, char_array, DATA_SIZE - 1); // 确保不会溢出，并保留最后的空终止符
        data_pkt.data[DATA_SIZE - 1] = '\0'; // 明确添加字符串结束符
        sendPacket(sockfd, data_pkt, server_addr);
        std::cout << "Sent data packet with seq " << seq_num << std::endl;
        // 等待 ACK
        ssize_t n = recvPacket(sockfd, pkt, server_addr);
        if (n > 0 && pkt.type == DATA_ACK && pkt.seq == seq_num) {
            std::cout << "Received ACK for seq " << seq_num << std::endl;
            seq_num++;
            break;
        } else {
            std::cerr << "No ACK or wrong ACK received, resending packet" << std::endl;
            // 重发数据报
            continue;
        }
    }

    // 这里已经可以断开连接了，实际上等服务端的 FIN-ASK 的时候，中间再去接受个文件也行
    // 发起连接终止
    Packet fin_pkt;
    fin_pkt.type = FIN;
    sendPacket(sockfd, fin_pkt, server_addr);
    std::cout << "Sent FIN to server" << std::endl;

    // 等待 FIN-ACK
    while (true) {
        ssize_t n = recvPacket(sockfd, recv_pkt, server_addr);
        if (n > 0 && recv_pkt.type == FIN_ACK) {
            std::cout << "Received FIN-ACK from server" << std::endl;
            break;
        }
    }

    // 接收服务器的信息
    uint32_t expected_seq = 0; // 期望收到的数据报数量
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, server_addr);
        if (n > 0 && pkt.type == DATA) {
            if (pkt.seq == expected_seq) {
                std::cout << "Received data packet with seq " << pkt.seq << " data is: " << pkt.data << std::endl;
                // 发送 DATA_ACK 表示收到数据报
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = pkt.seq;
                sendPacket(sockfd, ack_pkt, server_addr);
                std::cout << "Sent ACK for seq " << pkt.seq << std::endl;
                expected_seq++;
            } else {
                // 发送 ACK for last received packet
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = expected_seq - 1;
                sendPacket(sockfd, ack_pkt, server_addr);
                std::cerr << "Unexpected seq. Expected " << expected_seq
                          << ", but got " << pkt.seq << std::endl;
            }
        } else if (n == 0) {
            // 超时，继续等待
            continue;
        } else if (pkt.type == FIN) {
            // 客户端发起连接终止
            std::cout << "Received FIN from server" << std::endl;
            // 发送 FIN-ACK
            Packet fin_ack_pkt;
            fin_ack_pkt.type = FIN_ACK;
            sendPacket(sockfd, fin_ack_pkt, server_addr);
            std::cout << "Sent FIN-ACK to server" << std::endl;
            break;
        }
    }

    // 关闭套接字
    closesocket(sockfd);
    WSACleanup(); // 清理 Winsock
    std::cout << "Connection closed" << std::endl;
    return 0;
}