#pragma once

#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&,
                        Buffer*,
                        Timestamp)>;
// 高水位回调 防止接收方和发送方的速率不齐导致的数据解析错误或丢失
// 比如说到达水位线时发送方就可以停止发送
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;

void defaultConnectionCallback(const TcpConnectionPtr& conn);
void defaultMessageCallback(const TcpConnectionPtr& conn, 
                        Buffer* buffer,
                        Timestamp receiveTime);
                        