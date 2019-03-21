################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/net/CChannel.cpp \
../src/net/CIoContextPool.cpp \
../src/net/CProtocol.cpp \
../src/net/CServer.cpp \
../src/net/CSession.cpp \
../src/net/CSessionDb.cpp \
../src/net/CSessionMgr.cpp 

OBJS += \
./src/net/CChannel.o \
./src/net/CIoContextPool.o \
./src/net/CProtocol.o \
./src/net/CServer.o \
./src/net/CSession.o \
./src/net/CSessionDb.o \
./src/net/CSessionMgr.o 

CPP_DEPS += \
./src/net/CChannel.d \
./src/net/CIoContextPool.d \
./src/net/CProtocol.d \
./src/net/CServer.d \
./src/net/CSession.d \
./src/net/CSessionDb.d \
./src/net/CSessionMgr.d 


# Each subdirectory must supply rules for building sources it contributes
src/net/%.o: ../src/net/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -DNDEBUG -DBOOST_COROUTINES_NO_DEPRECATION_WARNING -DENABLE_BOOST_CONTEXT=ON -I../src -I/usr/local/include/ -O3 -rdynamic -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


