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

// ���ݰ��Ĵ�С���з����л������ݴ��� δ��ȡ/���͵������ڻ������洢
// ���������Ϊ�ġ�ճ�������� ��Ӧ�ò㴫������
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    // ������Ĭ�����ɶ���
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend+kInitialSize)
        , readerIndex_(kCheapPrepend)// ��ʼʱreadIndex��writeIndex��ͬһ��λ�� 
        , writerIndex_(kCheapPrepend)
    {}

    ~Buffer();

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // ��fd�϶�ȡ����ֱ�Ӵ���buffer������
    ssize_t readFd(int fd, int* savedRrrno);
    // ͨ��fd��������
    ssize_t writeFd(int fd, int* savedErrno);  
    // �޸�index
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

    // ��[data �� data+len]�ڴ��ϵ����� ��ӵ�writable������
    void append(const char* data,size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;

    }

private:
    // vector �ײ���Ԫ�صĵ�ַ
    char* begin()
    {
        // �ȵ�����*��������� ���ʵ���������Ԫ�ر��� �ڵ�����ȡ��ַ
        return &*buffer_.begin();
    }
    // ������ ���г�����ʱʹ��
    const char* begin() const 
    {
        return &*buffer_.begin();
    }

    // ���ػ������пɶ����ݵ���ʼ��ַ
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    // ��onMessage�����ϱ���buffer����ת��string���͵����ݷ��ظ��û�
    std::string retrieveAllAsString() 
    { 
        return retrieveAsString(readableBytes()); 
    }
    
    std::string retrieveAsString(size_t len) 
    { 
        assert(len<=readableBytes());
        std::string result(peek(), len); // �ѻ������пɶ��������Ѿ������ƣ����������
        retrieve(len); // ����Ҫ�Ի�������λ����
        return result;
    }

    // ��֤�����������
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
        // prependableBytes ���ص���readerIndex
        // �����ݵ�ʱ��һ��һ����ȫ������ ������index�Ƕ�̬��
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
            // ���м��read_data������ǰŲ�� ��ǰ���Ѿ���ȡ�˵Ŀ��еĻ�������������write_data
            assert(kCheapPrepend<readerIndex_);
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex_, // �׵�ַ
                    begin()+writerIndex_, // β��ַ
                    begin()+kCheapPrepend); // destination���׵�ַ
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());

        }
    }
    
    char* beginWrite(){ return begin() + writerIndex_; } // ���ؿ�д���׵�ַ
    const char* beginWrite() const { return begin() + writerIndex_; }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
