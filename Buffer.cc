#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
/*
    ��fd�϶�ȡ���� Poller������LTģʽ fd�ϵ�����û�ж���Ļ� �ײ��poller�᲻�ϵ��ϱ�
    ������LTģʽ�ĺô�֮һ�� �����ǲ��ᶪʧ��
    Buffer���������д�С�ģ����Ǵ�fd�϶�ȡ���ݵ�ʱ��ȴ��֪��tcp���ݵĴ�С ���𻺳����������ڴ��˷ѵ�����
*/

ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    // saved an ioctl()/FIONREAD call to tell how much to read
    char extrabuf[65536] = {0}; //  ջ�ϵ��ڴ�ռ� 64K
    // scatter read
    struct iovec vec[2];
    const size_t writable = writableBytes(); // ����Buffer�ײ㻺����ʣ��Ŀ�д�ռ��С
    // ��һ�������� �ڶ��� �����黺�������Ļ� �����extrabuf�Ͳ���Ҫ��
    vec[0].iov_base = begin()+ writerIndex_;
    vec[0].iov_len = writable;
    // �����һ�������������� ��ôscatter read��readv�����Զ� read into extrabuf
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof (extrabuf);
    /*------��߿ռ��ڴ��������--------*/
    // iovcnt ��ʾ�ж��ٸ�scattered buffer�ĸ���
    // ����ָ �������Buffer��Ŀ�д�ռ��С������extrabuf�Ŀռ��С ��ô�ͽ�extrabuf�Ŀռ�һ������ ����Ͳ�����
    const int iovcnt = (writable < sizeof (extrabuf))?2:1;
    const ssize_t n = ::readv(fd, vec, iovcnt);// �ڷ��������ڴ�ռ�д��ͬһ��fd�϶����������� ��Ч�ʸߣ�����
    if (n<0)
    {
        *savedErrno = errno;
    }
    else if (n <= writable)
    {
        // ��ʾBuffer����Ŀռ��Ѿ�����
        writerIndex_ += n;
    }
    else 
    {
        // ��ʾextrabuf����Ҳд��������
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