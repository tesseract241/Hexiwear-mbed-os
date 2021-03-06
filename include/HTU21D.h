#ifndef _HTU21D_driver_
#define _HTU21D_driver_
#include "HTU21D_enum.h"
#include "mbed.h"

#define TEMPERATURE_COEFFICIENT     (-0.15f)
#define HEAT_TRANSFER_COEFFICIENT   (1.f)

class HTU21D{

    public:
        HTU21D(HTU::Resolution resolution = HTU::H12_T14);
        float waitForTemperature();
        float waitForHumidity();
        float getTemperature();
        float getHumidity();

        void setResolution(HTU::Resolution resolution);
        void setTemperatureDelta(float temperature);
        void reset();


    private:
        DigitalOut mPower;
        I2C mI2C;
        uint8_t mAddress;
        HTU::Resolution mResolution;
        static const uint16_t CRC_PATTERN = 0b100110001;
        float mTemperature, temperatureDelta;

        float convertTemperature(uint8_t *rawTemperature);
        float convertHumidity(uint8_t *rawHumidity);
        bool checkForErrors(uint8_t *rawData);


};

#endif