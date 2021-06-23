# #!告诉系统其后路径所指定的程序即是解释此脚本文件的 Shell 程序  这里是bash
#!/bin/bash

#命令出现错误要报
set -e 

#如果没有build目录 创建给目录
if [! -d `pwd`/build]; then
    mkdir  `pwd`/build
fi

#clean up一下
rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. && #生成makefile文件
    make

#回到项目根目录
cd ..

#把头文件拷贝到 /usr/include/my_muduo so库拷贝到/usr/lib(在环境变量里)
if [! -d/usr/include/my_muduo]; then
    mkdir /usr/include/my_muduo
fi

for header in `ls *.h`
do
    cp $header /usr/include/my_muduo
done

cp `pwd`/lib/libmy_muduo.so /usr/lib

#如果so已经放到相应的路径下面了 但连接时还是报错找不着的话 执行ldconfig刷新动态库缓存
ldconfig