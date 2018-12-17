################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/thread/CThreadBase.cpp 

CC_SRCS += \
../src/thread/condition_variable.cc 

OBJS += \
./src/thread/CThreadBase.o \
./src/thread/condition_variable.o 

CC_DEPS += \
./src/thread/condition_variable.d 

CPP_DEPS += \
./src/thread/CThreadBase.d 


# Each subdirectory must supply rules for building sources it contributes
src/thread/%.o: ../src/thread/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../src -I../src/data -I../src/net -I../src/thread -I../src/util -I../src/openssl -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/thread/%.o: ../src/thread/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../src -I../src/data -I../src/net -I../src/thread -I../src/util -I../src/openssl -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


