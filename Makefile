
## c/c++ 代码编译Makefile
## zsm 2016/4/1
########################常规配置区###################################################
##目标文件
TARGET = TcpServer_Epoll_Duplex_Demo

##安装目录
INSTALL_DIR = /tmp

##包含头文件路径
# -I/xx/xx/include
INC_DIR = 

##依赖库
# -L/xx/xx/lib -lstd
LIB_DEPENDS = -lpthread

##编译选项
CFLAGS   = -Wall  -g -std=gnu99
CXXFLAGS = -Wall  -g -std=c++11
# -Wall  输出所有告警
# -O     在编译时进行优化
# -g     编译debug版本

######################################################################################
##编译器选用
CC = gcc
XX = g++

##产生一个所有.c .cpp文件列表
C_SRC = $(wildcard *.c)
CXX_SRC = $(wildcard *.cpp)

##产生一个所有.c 文件 对应的.o文件列表
#C_OBJS = $(C_SRC:%.c=%.o)
C_OBJS = $(subst .c,.o,$(C_SRC))
#CXX_OBJS = $(CXX_SRC:%.cpp=%.o)
CXX_OBJS = $(subst .cpp,.o,$(CXX_SRC))
ALL_OBJ = $(C_OBJS) $(CXX_OBJS)

##把所有.c文件生成.o文件
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INC_DIR)

##把所有.cpp文件生成.o文件
%.o:%.cpp
	$(XX) $(CXXFLAGS) -c $< -o $@ $(INC_DIR)

##目标文件,依赖OBJS中的.o文件
all: $(TARGET)
$(TARGET): $(ALL_OBJ)
	$(XX) $^ -o $(TARGET) $(INC_DIR) $(LIB_DEPENDS)
	chmod a+x $(TARGET)

##安装命令
install: all
	mkdir -p $(INSTALL_DIR)
	cp -R $(TARGET) $(INSTALL_DIR)

##执行make clean操作
clean:
	rm -rf *.o  $(TARGET)

.PHONY: all clean distclean install
