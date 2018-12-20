################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/util/CLogger.cpp \
../src/util/util.cpp 

OBJS += \
./src/util/CLogger.o \
./src/util/util.o 

CPP_DEPS += \
./src/util/CLogger.d \
./src/util/util.d 


# Each subdirectory must supply rules for building sources it contributes
src/util/%.o: ../src/util/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -DENABLE_BOOST_CONTEXT=ON -I../src -I../src/data -I../src/net -I../src/thread -I../src/util -I../src/openssl -I/usr/local/include/ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


