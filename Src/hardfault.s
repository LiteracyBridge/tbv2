
;********  hardfault.s
;   JEB  Feb-2020
;   define a fault handler that saves current stack pointer, then calls
;        HardFault_Handler_C( stackpointer, linkreg )    
;   overrides default looping handlers from startup_stm32fxxxx_xl.s 
;    for unrecoverable faults
;************************************************************ JEB
                AREA    |.text|, CODE, READONLY
                EXTERN  HardFault_Handler_C     ; JEB
NMI_Handler             
HardFault_Handler
MemManage_Handler
BusFault_Handler
UsageFault_Handler 
                EXPORT  NMI_Handler           
                EXPORT  HardFault_Handler           
                EXPORT  MemManage_Handler           
                EXPORT  BusFault_Handler           
                EXPORT  UsageFault_Handler           
                TST     LR, #4                  ;JEB    test bit-2 of LR to determine if fault was stored on MSP or PSP 
                MRS     R0, MSP                 ;JEB    MainStackPointer to R0 (if fault on main stack)
                MRSNE   R0, PSP                 ;JEB    ProcessStackPointer to R0 (if fault was on a thread stack)
                MOV     R1, LR                  ;JEB    pass Link Reg also
                B       HardFault_Handler_C     ;JEB    HardFault_Handler_C(uint32 *stack, uint32 icsr)
                BKPT    #1                      ;JEB    
                B       .                       ;JEB

                END
;  end hardfault.s
