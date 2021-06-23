#pragma once

#include <vector>
#include <string>
#include <algorithm> // std::copy()
#include <cassert>

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode

// 根据包的大小进行反序列化的数据处理 未读取/发送的数据在缓冲区存储
// 用来解决作为的“粘包”问题 即应用层传输问题
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    // 不允许默认生成对象
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend+kInitialSize)
        , readerIndex_(kCheapPrepend)// 初始时readIndex与writeIndex在同一个位置 
        , writerIndex_(kCheapPrepend)
    {}

    ~Buffer();

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // 从fd上读取数据直接存在buffer里面了
    ssize_t readFd(int fd, int* savedRrrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* savedErrno);  
    // 修改index
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ += len;
        }
        else 
        {
            retrieveAll();
        }
    }  

    // 把[data ， data+len]内存上的数据 添加到writable缓冲区
    void append(const char* data,size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;

    }

private:
    // vector 底层首元素的地址
    char* begin()
    {
        // 先调用了*运算符重载 访问的是容器的元素本身 在调用了取地址
        return &*buffer_.begin();
    }
    // 常方法 当有常对象时使用
    const char* begin() const 
    {
        return &*buffer_.begin();
    }

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    // 把onMessage函数上报的buffer数据转成string类型的数据返回给用户
    std::string retrieveAllAsString() 
    { 
        return retrieveAsString(readableBytes()); 
    }
    
    std::string retrieveAsString(size_t len) 
    { 
        assert(len<=readableBytes());
        std::string result(peek(), len); // 把缓冲区中可读的数据已经（复制）构造出来了
        retrieve(len); // 这里要对缓冲区复位操作
        return result;
    }

    // 保证缓冲区不溢出
    void ensureWritableBytes(size_t len) 
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes()>=len);
    }

    void makeSpace(size_t len)
    {
        // prependableBytes 返回的是readerIndex
        // 读数据的时候不一定一次性全部读完 这两个index是动态的
        /*
        kCheapPrepend | reader  | writer  |
        kCheapPrepend |           len        |
        */
        if(writableBytes() + prependableBytes()< len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_+len);
        }
        else
        {
            //move readable data to the front, make space inside buffer
            // 将中间的read_data部分往前挪动 将前面已经读取了的空闲的缓冲区补给后面write_data
            assert(kCheapPrepend<readerIndex_);
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex_, // 首地址
                    begin()+writerIndex_, // 尾地址
                    begin()+kCheapPrepend); // destination的首地址
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());

        }
    }
    
    char* beginWrite(){ return begin() + writerIndex_; } // 返回可写的首地址
    const char* beginWrite() const { return begin() + writerIndex_; }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
