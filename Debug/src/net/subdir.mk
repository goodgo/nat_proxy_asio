################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/net/CChannel.cpp \
../src/net/CIoContextPool.cpp \
../src/net/CProtocol.cpp \
../src/net/CQueue.cpp \
../src/net/CServer.cpp \
../src/net/CSession.cpp 

OBJS += \
./src/net/CChannel.o \
./src/net/CIoContextPool.o \
./src/net/CProtocol.o \
./src/net/CQueue.o \
./src/net/CServer.o \
./src/net/CSession.o 

CPP_DEPS += \
./src/net/CChannel.d \
./src/net/CIoContextPool.d \
./src/net/CProtocol.d \
./src/net/CQueue.d \
./src/net/CServer.d \
./src/net/CSession.d 


# Each subdirectory must supply rules for building sources it contributes
src/net/%.o: ../src/net/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -DENABLE_BOOST_CONTEXT=ON -I../src -I../src/data -I../src/net -I../src/thread -I../src/util -I../src/openssl -I/usr/local/include/ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


