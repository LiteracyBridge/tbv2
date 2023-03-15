@echo on
@rem --target=arm-arm-none-eabi
set PP="C:\Keil_v5\ARM\ARMCLANG\Bin\ArmClang.exe" --target=arm-arm-none-eabi -I .\Src\inc -E
set INPUT_H_DIR=.\Src\inc
set JAVA_SRC_DIR=..\CSMcompile\src\main\java\org\amplio\csm\
set JAVA_SRC_IN=%JAVA_SRC_DIR%CsmEnums.h
set JAVA_SRC_OUT=%JAVA_SRC_DIR%CsmEnums.java

%PP% %JAVA_SRC_IN% |grep -vE '^#'>%JAVA_SRC_OUT%
