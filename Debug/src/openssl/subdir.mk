################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/openssl/COpenssl.cpp 

OBJS += \
./src/openssl/COpenssl.o 

CPP_DEPS += \
./src/openssl/COpenssl.d 


# Each subdirectory must supply rules for building sources it contributes
src/openssl/%.o: ../src/openssl/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../src -I../src/data -I../src/net -I../src/thread -I../src/util -I../src/openssl -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


