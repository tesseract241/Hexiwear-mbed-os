#ifndef _FXAS21002C_Driver_
#define _FXAS21002C_Driver_

#include "mbed.h"
#include "FXAS21002C_enum.h"


class FXAS21002C{

    public:
        FXAS21002C(FXA::Range range = FXA::RANGE2000, FXA::ODR dataRate = FXA::ODR100);

        typedef struct{
            FXA::Axis axis;
            float value;
        } mail_t;
        Mail<mail_t, 24> mailBox;

        void standby();
        void setActive();
        void setReady();
        FXA::Mode getStatus();
        bool isDataAvailable();

        void setRange(FXA::Range range);
        void setLowPass(FXA::Low threshold);
        void setHighPass(FXA::High threshold);
        void setODR(FXA::ODR dataRate);

        float* getAngles();
        float* getRadians();
        int8_t* getTemperature();
        void setInterrupt(FXA::Interrupt_Pin pin, FXA::Interrupt name, void (*function)(), float threshold = 0, int count = 0, bool resetCount = false);
        void removeInterrupt(FXA::Interrupt name);
        

    private:
        I2C mI2C;
        uint8_t mAddress;
        uint8_t activeInterruptsOne;
        uint8_t activeInterruptsTwo;
        float mSensitivity;
        float mODR;
        InterruptIn mInterruptOne;
        InterruptIn mInterruptTwo;
        Thread mThreadOne;
        Thread mThreadTwo;
        void interruptWrapper(FXA::Interrupt_Pin pin);
        void interruptWrapperOne();
        void interruptWrapperTwo();
        void (*FXAS21002CInterruptOne)();
        void (*FXAS21002CInterruptTwo)();

        int16_t* getRawData();
        void dispatchInterruptData(FXA::Interrupt_Pin pin);
        void dispatchInterruptDataOne();
        void dispatchInterruptDataTwo();
        void setInterruptFunction(void (*function)(), FXA::Interrupt_Pin pin);
        FXA::Interrupt identifyInterrupt(FXA::Interrupt_Pin pin);

        float convertToAngle(int16_t rawAngle);
        float convertToRadian(int16_t rawAngle);

        void read(FXA::Address address, uint8_t *data, int length = 1);
        int write(FXA::Address address, uint8_t *data, int length = 1);

};


#endif