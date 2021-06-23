#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
/*
    从fd上读取数据 Poller工作在LT模式 fd上的数据没有读完的话 底层的poller会不断地上报
    所以用LT模式的好处之一： 数据是不会丢失的
    Buffer缓冲区是有大小的，但是从fd上读取数据的时候却不知道tcp数据的大小 引起缓冲区不够或内存浪费的问题
*/

ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    // saved an ioctl()/FIONREAD call to tell how much to read
    char extrabuf[65536] = {0}; //  栈上的内存空间 64K
    // scatter read
    struct iovec vec[2];
    const size_t writable = writableBytes(); // 这是Buffer底层缓冲区剩余的可写空间大小
    // 第一个缓冲区 在堆上 如果这块缓冲区够的话 下面的extrabuf就不需要了
    vec[0].iov_base = begin()+ writerIndex_;
    vec[0].iov_len = writable;
    // 如果第一个缓冲区不够填 那么scatter read（readv）会自动 read into extrabuf
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof (extrabuf);
    /*------提高空间内存的利用率--------*/
    // iovcnt 表示有多少个scattered buffer的个数
    // 这里指 如果现在Buffer里的可写空间大小少于了extrabuf的空间大小 那么就将extrabuf的空间一并算上 否则就不算了
    const int iovcnt = (writable < sizeof (extrabuf))?2:1;
    const ssize_t n = ::readv(fd, vec, iovcnt);// 在非连续的内存空间写入同一个fd上读出来的数据 （效率高？！）
    if (n<0)
    {
        *savedErrno = errno;
    }
    else if (n <= writable)
    {
        // 表示Buffer本身的空间已经够了
        writerIndex_ += n;
    }
    else 
    {
        // 表示extrabuf里面也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n-writable);
    }

    return n;

}

ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
     ssize_t n = ::write(fd, peek(), readableBytes());
     if (n<0)
     {
         *savedErrno = errno;
     }
     return n;
}   