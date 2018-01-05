#include "SSD1351_enum.h"
#include "SSD1351.h"
#include <algorithm>
#include <cmath>
#include "SSD1351_Fonts.h"

uint8_t SSD1351::xStartActive = 0;
uint8_t SSD1351::xEndActive = SCREEN_SIZE - 1; 
uint8_t SSD1351::yStartActive = 0; 
uint8_t SSD1351::yEndActive = SCREEN_SIZE - 1;
uint8_t SSD1351::xStartDrawing = SCREEN_SIZE - 1;
uint8_t SSD1351::xEndDrawing = 0; 
uint8_t SSD1351::yStartDrawing = SCREEN_SIZE - 1; 
uint8_t SSD1351::yEndDrawing = 0;


SSD1351::SSD1351() : mSPI(PTB22, NC, PTB21, PTB20), dataORcommand(PTD15), mPower(PTC13), resetPin(PTE6), chipSelect(PTB20){
    mSPI.frequency(8000000);
    mSPI.format(8, 3);
    reset();
    screenBuffer1 = new uint16_t[SCREEN_SIZE * SCREEN_SIZE];
    screenBuffer2 = new uint16_t[SCREEN_SIZE * SCREEN_SIZE];
    activeScreenBuffer = &screenBuffer2;
    drawingScreenBuffer = &screenBuffer1;
    fill(screenBuffer1, screenBuffer1 + SCREEN_SIZE*SCREEN_SIZE, 0);
    fill(screenBuffer2, screenBuffer2 + SCREEN_SIZE*SCREEN_SIZE, 0);
    mThread.start(callback(this, &SSD1351::workerDraw));
    unlockCommands();
    write(SSD_SLEEP);
    uint8_t *data = new uint8_t[3];
    data[0] = 0xF1;
    write(SSD_FPS);
    write(data);
    data[0] = 0;
    data[1] = SCREEN_SIZE-1;
    write(SSD_ACTIVE_ROWS);
    write(data+1);
    data[0] = 0x60;
    write(SSD_OPTIONS);
    write(data);
    data[0] = X_OFFSET;
    data[1] = SCREEN_SIZE - 1 + X_OFFSET;
    write(SSD_SELECT_HORIZONTAL);
    write(data, 2);
    data[0] = Y_OFFSET;
    data[1] = SCREEN_SIZE - 1 + Y_OFFSET;
    write(SSD_SELECT_VERTICAL);
    write(data, 2); 
    data[0] = 0x5F;
    write(SSD_IMAGE_V_OFFSET);
    write(data);
    data[0] = 0x80;
    write(SSD_DISPLAY_V_OFFSET);
    write(data); 
    setChargePeriods(0x32);
    write(SSD_DISPLAY_ON); 
    setBrightness(0xCF);
    data[0] = 0x01;
    write(SSD_PHASE_3);
    write(data);
    delete[] data;
    write(SSD_WAKE_UP);
}


SSD1351::~SSD1351(){
    // TODO delete[] whatever we have new[]
    mThread.terminate();
    powerOff();
}

void SSD1351::powerUp(){
    resetPin = 0;
    wait_ms(2);
    resetPin = 1;
    wait_ms(1);
    mPower = 1;
    wait_ms(1);
    write(SSD_WAKE_UP);
    wait_ms(200);
}

void SSD1351::powerOff(){
    write(SSD_SLEEP);
    mPower = 0;
    wait_ms(1);
}

void SSD1351::reset(){
    powerOff();
    powerUp();
}

void SSD1351::sleep(){
    uint8_t data = 0;
    write(SSD_SLEEP);
    write(SSD_SLEEP_EXTRA);
    write(&data);
}

void SSD1351::wakeUp(){
    uint8_t data = 1;
    write(&data);
    write(SSD_SLEEP_EXTRA);
    write(SSD_WAKE_UP);
}


void SSD1351::setBrightness(uint8_t brightness){
    brightness%=17;
    write(SSD_BRIGHTNESS);
    write(&brightness);
}

void SSD1351::setColourBrightness(uint8_t red, uint8_t green, uint8_t blue){
    uint8_t *brightness = new uint8_t[3];
    brightness[0] = red;
    brightness[1] = green;
    brightness[2] = blue;
    write(SSD_COLOUR_BRIGTHNESS);
    write(brightness, 3);
    delete[] brightness;
}

void SSD1351::draw(bool keepScreen){
    while(mThread.get_state() != Thread::WaitingThreadFlag) ;
    if(*activeScreenBuffer == screenBuffer1){
        activeScreenBuffer  = &screenBuffer2;
        drawingScreenBuffer = &screenBuffer1;
    }
    else{
        activeScreenBuffer  = &screenBuffer1;
        drawingScreenBuffer = &screenBuffer2;
    }
    xStartDrawing = xStartActive;
    xEndDrawing = xEndActive;
    yStartDrawing = yStartActive;
    yEndDrawing = yEndActive;
    if(keepScreen){
        memcpy((void*) *activeScreenBuffer, (void*) *drawingScreenBuffer, 2*SCREEN_SIZE*SCREEN_SIZE);
        xStartActive = 0;
        xEndActive = SCREEN_SIZE - 1;
        yStartActive = 0;
        yEndActive = SCREEN_SIZE - 1;
    }
    else{clearScreen();}
    mThread.signal_set(0x01);
}

void SSD1351::drawAndKeep(){
    draw(true);
}

void SSD1351::drawAndClear(){
    draw(false);
}

void SSD1351::clearScreen(){
    xStartActive = 0;
    xEndActive = SCREEN_SIZE - 1;
    yStartActive = 0;
    yEndActive = SCREEN_SIZE - 1;
    fill(*activeScreenBuffer, *activeScreenBuffer + SCREEN_SIZE*SCREEN_SIZE, 0);
}

SSD_Error SSD1351::addText(uint8_t xPosition, uint8_t yPosition, char* text, uint16_t textLength, bool topOrBottom, TextProperties_t textProperties){
    Font* currentFont = fontDatabase[textProperties.font].fontCollection[textProperties.size];
    SSD_Error error = boundaryCheck(xPosition, yPosition, calculateTextSpace(text, textLength, textProperties), textProperties.size);
    //TODO Need to consider if and when to split into lines, and how to calculate them.
    for(uint16_t i=0;i<textLength;i++){
        uint8_t currentIndex = ((uint8_t) text[i]) - FONT_OFFSET;
        for(uint j=0;j<currentFont->width[currentIndex] * currentFont->height[currentIndex];j++){
            if(!topOrBottom && (*activeScreenBuffer)[xPosition + j%currentFont->width[currentIndex] + currentFont->xPosition[currentIndex] + (yPosition + div(j, currentFont->width[currentIndex]).quot + currentFont->yPosition[currentIndex])*SCREEN_SIZE]){}
            else{    
                (*activeScreenBuffer)   [
                                        xPosition + j%currentFont->width[currentIndex] + currentFont->xPosition[currentIndex]
                                         + (yPosition + div(j, currentFont->width[currentIndex]).quot + currentFont->yPosition[currentIndex])*SCREEN_SIZE
                                        ] = 
                textProperties.colour * (currentFont->alphabet[currentIndex][j]);
            }
        }
    }
    return error;
}


SSD_Error SSD1351::addLine(uint8_t xPosition, uint8_t yPosition, uint8_t length, uint16_t angle, uint16_t internalColour, uint8_t internalThickness, bool topOrBottom, uint16_t externalColour, uint8_t externalThickness){
    SSD_Error error, tempError;
    uint8_t internalLength, counter, counter2;
    int8_t transversalDelta, tangentialDelta; 
    calculateLineParameters(angle, length, internalLength, counter, counter2, tangentialDelta, transversalDelta);
    int16_t delta = 0;
    int16_t coordinates = xPosition + SCREEN_SIZE * yPosition;
    float sinAngle = sin(angle * M_PI/180);
    float cosAngle = cos(angle * M_PI/180);
    for(uint8_t i=0;i<internalThickness + externalThickness;i++){
        tempError = boundaryCheck(div(coordinates+delta, SCREEN_SIZE).rem, div(coordinates+delta, SCREEN_SIZE).quot, lround(length*abs(cosAngle)), lround(length*abs(sinAngle)));
        if(tempError==SSD_NO_ERROR){
            if(i<internalThickness){addLineInternal(coordinates + delta, internalLength, tangentialDelta, transversalDelta, counter, counter2, internalColour, topOrBottom);}
            else{addLineInternal(coordinates + delta, internalLength, tangentialDelta, transversalDelta, counter, counter2, externalColour, topOrBottom);}
            }
        else{error=tempError;}
        tempError = boundaryCheck(div(coordinates-delta, SCREEN_SIZE).rem, div(coordinates-delta, SCREEN_SIZE).quot, lround(length*abs(cosAngle)), lround(length*abs(sinAngle)));
        if(tempError==SSD_NO_ERROR){
            if(i<internalThickness){addLineInternal(coordinates - delta, internalLength, tangentialDelta, transversalDelta, counter, counter2, internalColour, topOrBottom);}
            else{addLineInternal(coordinates - delta, internalLength, tangentialDelta, transversalDelta, counter, counter2, externalColour, topOrBottom);}
        }
        else{error=tempError;}
        if(abs(angle%90)>CRITICAL_ANGLE_MIN && abs(angle%90)<CRITICAL_ANGLE_MAX){
            delta-=transversalDelta;
        }
        else{delta+=transversalDelta;}
    }
   return error;
}

SSD_Error SSD1351::addLineOnTop(uint8_t xPosition, uint8_t yPosition, uint8_t length, uint16_t angle, uint16_t internalColour, uint8_t internalThickness, uint16_t externalColour, uint8_t externalThickness){
    return addLine(xPosition, yPosition, length, angle, internalColour, internalThickness, true, externalColour, externalThickness);
}

SSD_Error SSD1351::addLineAtBottom(uint8_t xPosition, uint8_t yPosition, uint8_t length, uint16_t angle, uint16_t internalColour, uint8_t internalThickness, uint16_t externalColour, uint8_t externalThickness){
    return addLine(xPosition, yPosition, length, angle, internalColour, internalThickness, false, externalColour, externalThickness);
}


SSD_Error SSD1351::addImage(uint16_t *image, uint8_t xPosition, uint8_t yPosition, uint8_t width, uint8_t height, bool topOrBottom){
    SSD_Error result = boundaryCheck(xPosition, yPosition, width, height);
    if(result){return result;}
    for(uint i=xPosition;i<=xPosition+width;i++){
        for(uint j=yPosition;j<=yPosition+height;j++){
            if(!topOrBottom && (*activeScreenBuffer)[j*SCREEN_SIZE + i]){continue;}
            else{(*activeScreenBuffer)[j*SCREEN_SIZE + i] = image[(j - yPosition)*width + i - xPosition];}
        }
    }
    return SSD_NO_ERROR;
}

SSD_Error SSD1351::addImageOnTop(uint16_t *image, uint8_t xPosition, uint8_t yPosition, uint8_t width, uint8_t height){
    return addImage(image, xPosition, yPosition, width, height, true);
}

SSD_Error SSD1351::addImageAtBottom(uint16_t *image, uint8_t xPosition, uint8_t yPosition, uint8_t width, uint8_t height){
    return addImage(image, xPosition, yPosition, width, height, false);
    
}

void SSD1351::scrollingOn(bool direction, uint8_t startingRow, uint8_t numberOfRows, SSD_Scrolling_Period period){
    uint8_t *data = new uint8_t[5];
    if(direction){data[0] = 1;}
    else{data[0] = 64;}
    data[1] = min(startingRow, (uint8_t)(SCREEN_SIZE - 1));
    data[2] = min(numberOfRows, (uint8_t)(SCREEN_SIZE - data[1]));
    data[3] = 0;
    data[4] = period;
    write(SSD_SCROLLING);
    write(data, 5);
    write(SSD_SCROLLING_START);
    delete[] data;
}

void SSD1351::scrollingOff(){
    write(SSD_SCROLLING_STOP);
}

uint16_t SSD1351::getColour(uint8_t red, uint8_t green, uint8_t blue){
    return green>>3 | (red>>2)<<5 | (blue>>3)<<11;
}


void SSD1351::lock(){
    uint8_t data = 16;
    write(SSD_COMMAND_LOCK);
    write(&data);
}

void SSD1351::unlock(){
    uint8_t data = 12;
    write(SSD_COMMAND_LOCK);
    write(&data);
}

void SSD1351::lockCommands(){
    unlock();
    uint8_t data = 0xB0;
    write(SSD_COMMAND_LOCK);
    write(&data);
}

void SSD1351::unlockCommands(){
    unlock();
    uint8_t data = 0xB1;
    write(SSD_COMMAND_LOCK);
    write(&data);
}

void SSD1351::setChargePeriods(uint8_t periods){
    unlockCommands();
    write(SSD_PHASE_1_AND_2);
    write(&periods);
    lockCommands();
}


void SSD1351::workerDraw(){
    while(1){
        Thread::signal_wait(0x01);
        uint8_t *data = new uint8_t[2];
        data[0] = xStartDrawing + X_OFFSET;
        data[1] = xEndDrawing + X_OFFSET;
        write(SSD_SELECT_HORIZONTAL);
        write(data, 2);
        data[0] = yStartDrawing + Y_OFFSET;
        data[1] = yEndDrawing + Y_OFFSET;
        write(SSD_SELECT_VERTICAL);
        write(data, 2);
        delete[] data;
        write(SSD_WRITE_ENABLE);
        mMutex.lock();
        for(uint8_t i=yStartDrawing;i<yEndDrawing || i==yStartDrawing;i++){
            write((uint8_t *)(*drawingScreenBuffer + i*SCREEN_SIZE + xStartDrawing),
            2 * (xEndDrawing-xStartDrawing+1));
        }
        mMutex.unlock();
    }
}





SSD_Error SSD1351::boundaryCheck(uint8_t xPosition, uint8_t yPosition, int16_t deltaX, int16_t deltaY){
    int16_t minX = (xPosition < xPosition + deltaX) ? xPosition : xPosition + deltaX;
    int16_t maxX = (xPosition > xPosition + deltaX) ? xPosition : xPosition + deltaX;
    int16_t minY = (yPosition < yPosition + deltaY) ? yPosition : yPosition + deltaY;
    int16_t maxY = (yPosition > yPosition + deltaY) ? yPosition : yPosition + deltaY;
    if(minX < 0)                {return SSD_OUT_OF_LEFT_BORDER;}
    if(maxX > SCREEN_SIZE - 1)  {return SSD_OUT_OF_RIGHT_BORDER;}   
    if(minY < 0)                {return SSD_OUT_OF_TOP_BORDER;}             
    if(maxY > SCREEN_SIZE - 1)  {return SSD_OUT_OF_BOTTOM_BORDER;}
    
    if(minX < xStartActive)  {xStartActive = minX;}
    if(maxX > xEndActive)    {xEndActive   = maxX;}
    if(minY < yStartActive)  {yStartActive = minY;}
    if(maxY > yEndActive)    {yEndActive   = maxY;}

    return SSD_NO_ERROR;
}

void SSD1351::addLineInternal(int16_t coordinates, uint8_t length, int8_t tangentialDelta, int8_t transversalDelta, int8_t counter, int8_t counter2, uint16_t colour,  bool topOrBottom){
    for(uint8_t i=0;i<length;i++){
        if(i){
            if(i%counter==0){
                coordinates+=transversalDelta;
                if(counter2!=length){   
                    int8_t counterDummy = counter;
                    counter = counter2;
                    counter2 = counterDummy;
                }
            }
            coordinates+=tangentialDelta;
        }
        if(!topOrBottom && (*activeScreenBuffer)[coordinates]){}
        else{(*activeScreenBuffer)[coordinates] = colour;}
    }
}

void SSD1351::calculateLineParameters(const uint16_t angle, const uint8_t length, uint8_t &internalLength, uint8_t &counter, uint8_t &counter2,  int8_t &tangentialDelta, int8_t &transversalDelta){
    // This identifies where the line is compared to the diagonal lines, which is used to determine 
    // whether the majority of the pixels will be shifted vertically, horizontally 
    // or diagonally compared to the previous ones.
    float sinAngle = sin(angle * M_PI/180);
    float cosAngle = cos(angle * M_PI/180);
    if(abs(sinAngle)>abs(cosAngle)){
        internalLength = length*abs(sinAngle);
        transversalDelta = copysign(1, cosAngle);
        tangentialDelta = copysign(SCREEN_SIZE, sinAngle);
        if(abs(cosAngle)>0.001){
            counter = abs(lround(sinAngle/cosAngle));
            counter2 = abs(lround(2*sinAngle/cosAngle));
            if(counter2==2*counter){counter2 = internalLength;}
        }
        else{
            counter = internalLength;
            counter2 = internalLength;
        }
    }
    else{
        internalLength = length*abs(cosAngle);
        transversalDelta = copysign(SCREEN_SIZE, sinAngle);
        tangentialDelta = copysign(1, cosAngle);
        if(abs(sinAngle)>0.001){
            counter = abs(lround(cosAngle/sinAngle));
            counter2 = abs(lround(2*cosAngle/sinAngle));
            if(counter2==2*counter){counter2 = internalLength;}
        }
        else{
            counter = internalLength;
            counter2 = internalLength;
        }
    }
    if(abs(angle%90)>CRITICAL_ANGLE_MIN && abs(angle%90)<CRITICAL_ANGLE_MAX){
            if(sinAngle/cosAngle>0){
                counter = abs(lround(1/std::tan((angle - 45) * M_PI/180)));
                counter2 = abs(lround(2/std::tan((angle - 45) * M_PI/180)));
            }
            else{
                counter = abs(lround(std::tan((angle - 45) * M_PI/180)));
                counter2 = abs(lround(2*std::tan((angle - 45) * M_PI/180)));
            }
            if(counter2==2*counter){counter2 = internalLength;}
            tangentialDelta+=transversalDelta;
            transversalDelta= -transversalDelta;
            if(abs(angle%90)==45){
            counter = internalLength;
            }
    }
}

uint16_t SSD1351::calculateTextSpace(char *text, uint16_t textLength, TextProperties_t textProperties){
    uint16_t result;
    for(uint16_t i=0;i<textLength; i++){
        result+=(fontDatabase[textProperties.font].fontCollection[textProperties.size])->width[((uint8_t) text[i]) - FONT_OFFSET];
    }
    return result; 
}


int SSD1351::write(SSD_Command command){
    dataORcommand = 0;
    int result = mSPI.write((uint8_t) command);
    return result;
}

int SSD1351::write(uint8_t *data, int length){
    int result, tempResult = 0;
    dataORcommand = 1;
    for(uint8_t i=0;i<length;i++){
        tempResult = mSPI.write(*(data+i));
        if(tempResult){
            result = tempResult;
            break;
        }
    }
    return result;
}


