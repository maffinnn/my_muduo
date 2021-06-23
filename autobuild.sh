# #!����ϵͳ���·����ָ���ĳ����ǽ��ʹ˽ű��ļ��� Shell ����  ������bash
#!/bin/bash

#������ִ���Ҫ��
set -e 

#���û��buildĿ¼ ������Ŀ¼
if [! -d `pwd`/build]; then
    mkdir  `pwd`/build
fi

#clean upһ��
rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. && #����makefile�ļ�
    make

#�ص���Ŀ��Ŀ¼
cd ..

#��ͷ�ļ������� /usr/include/my_muduo so�⿽����/usr/lib(�ڻ���������)
if [! -d/usr/include/my_muduo]; then
    mkdir /usr/include/my_muduo
fi

for header in `ls *.h`
do
    cp $header /usr/include/my_muduo
done

cp `pwd`/lib/libmy_muduo.so /usr/lib

#���so�Ѿ��ŵ���Ӧ��·�������� ������ʱ���Ǳ����Ҳ��ŵĻ� ִ��ldconfigˢ�¶�̬�⻺��
ldconfig