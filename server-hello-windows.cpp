#include <rudp.h>
#include <cstring> // 为 strncpy 引入头文件
#include <iostream>

// 这是服务端的实现，为了方便，这里没有考虑多客户机的情况。
// 如果要使用多客户机，可以添加pthread
// 在接收到SYN之后，转到新的线程去处理这个客户机即可
// 主要是懒得搞了，反正效果差不多
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./server <port>" << std::endl;
        return -1;
    }

    int port = atoi(argv[1]);

    SOCKET sockfd;
    sockaddr_in server_addr{}, client_addr{};

    // 初始化 WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    // 创建 UDP 套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return -1;
    }

    // 绑定套接字
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    // 等待连接
    std::cout << "Server listening on port " << port << std::endl;

    // 连接建立 (三次握手)
    Packet pkt;
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);

        // 收到建立连接的请求
        if (n > 0 && pkt.type == SYN) {
            std::cout << "Received SYN from client" << std::endl;
            // 发送 SYN-ACK
            Packet syn_ack_pkt;
            syn_ack_pkt.type = SYN_ACK;
            syn_ack_pkt.seq = pkt.seq + 1;
            sendPacket(sockfd, syn_ack_pkt, client_addr);
            std::cout << "Sent SYN-ACK to client" << std::endl;

            // 等待 ACK
            n = recvPacket(sockfd, pkt, client_addr);
            if (n > 0 && pkt.type == ACK) {
                std::cout << "Received ACK from client" << std::endl;
                break; // 连接建立
            }
        }
    }

    // 这里可以做任何事，比如简单接受一个hello，再传回去一个hello
    uint32_t expected_seq = 0; // 期望收到的数据报数量
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);
        if (n > 0 && pkt.type == DATA) {
            if (pkt.seq == expected_seq) {
                std::cout << "Received data packet with seq " << pkt.seq << " data is: " << pkt.data << std::endl;
                // 发送 DATA_ACK 表示收到数据报
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = pkt.seq;
                sendPacket(sockfd, ack_pkt, client_addr);
                std::cout << "Sent ACK for seq " << pkt.seq << std::endl;
                expected_seq++;
            } else {
                // 发送 ACK for last received packet
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = expected_seq - 1;
                sendPacket(sockfd, ack_pkt, client_addr);
                std::cerr << "Unexpected seq. Expected " << expected_seq
                          << ", but got " << pkt.seq << std::endl;
            }
        } else if (n == 0) {
            // 超时，继续等待
            continue;
        } else if (pkt.type == FIN) {
            // 客户端发起连接终止
            std::cout << "Received FIN from client" << std::endl;
            // 发送 FIN-ACK
            Packet fin_ack_pkt;
            fin_ack_pkt.type = FIN_ACK;
            sendPacket(sockfd, fin_ack_pkt, client_addr);
            std::cout << "Sent FIN-ACK to client" << std::endl;
            break;
        }
    }

    // 发送hello给客户端
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
        sendPacket(sockfd, data_pkt, client_addr);
        std::cout << "Sent data packet with seq " << seq_num << std::endl;
        // 等待 ACK
        ssize_t n = recvPacket(sockfd, pkt, client_addr);
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

    // 连接终止 (四次挥手)
    Packet fin_pkt;
    fin_pkt.type = FIN;
    sendPacket(sockfd, fin_pkt, client_addr);
    std::cout << "Sent FIN to client" << std::endl;

    // 等待 FIN-ACK
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);
        if (n > 0 && pkt.type == FIN_ACK) {
            std::cout << "Received FIN-ACK from client" << std::endl;
            break;
        }
    }

    closesocket(sockfd);
    WSACleanup(); // 清理 WinSock
    std::cout << "Connection closed" << std::endl;
    return 0;
}
