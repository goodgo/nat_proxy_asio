################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/main.cpp 

OBJS += \
./src/main.o 

CPP_DEPS += \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	/opt/rh/devtoolset-7/root/usr/bin/g++ -std=c++0x -DBOOST_COROUTINES_NO_DEPRECATION_WARNING -I/opt/rh/devtoolset-7/root/usr/include/c++/7 -I/opt/rh/devtoolset-7/root/usr/local/boost1_68/include/ -I../src -O0 -g -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


