#include "UWB_DW1000.h"
#include "DW1000Ng.hpp"

UWB_DW1000* UWB_DW1000::_instance = nullptr;

UWB_DW1000::UWB_DW1000() : _cs(-1), _irq(-1), _rst(-1), txDoneCallback(nullptr), rxDoneCallback(nullptr) {
    _instance = this;
}

bool UWB_DW1000::begin(int csPin, int irqPin, int rstPin, SPIClass& spi) {
    (void)spi;
    _cs = csPin;
    _irq = irqPin;
    _rst = rstPin;

    DW1000Ng::initializeNoInterrupt(_cs, _rst);
    
    // Verify SPI communication by checking Device ID
    char msg[128];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    // The device ID should start with DECA, which translates to a specific string format.
    // Actually, let's just do a simple check: if it reads all 0s or all FFs, it's bad.
    // In DW1000Ng, we can just check if we can read something valid.
    // The printable identifier for DW1000 is usually "0130 - model: 1, version: 3, revision: 0"
    if (strstr(msg, "00 - model: 0") != nullptr || strstr(msg, "FFFF") != nullptr || strstr(msg, "FF - ") != nullptr) {
        return false;
    }

    // We attach our own interrupt handler since we want a unified interface
    pinMode(_irq, INPUT);
    // Normally DW1000Ng handles this, but we'll use their hooks:
    DW1000Ng::attachSentHandler(handleSent);
    DW1000Ng::attachReceivedHandler(handleReceived);
    DW1000Ng::attachReceiveFailedHandler(handleReceiveFailed);
    DW1000Ng::attachReceiveTimeoutHandler(handleReceiveTimeout);
    
    // Call configure with default parameters to apply interrupts and DW1000 settings
    return configure();
}

bool UWB_DW1000::configure(uint8_t channel, uint8_t preambleCode, uint16_t preambleLength, uint8_t dataRate) {
    device_configuration_t config;
    config.extendedFrameLength = false;
    config.receiverAutoReenable = false;
    config.smartPower = false;
    config.frameCheck = true;
    config.nlos = false;
    config.sfd = SFDMode::STANDARD_SFD;
    
    if (channel == 1) config.channel = Channel::CHANNEL_1;
    else if (channel == 2) config.channel = Channel::CHANNEL_2;
    else if (channel == 3) config.channel = Channel::CHANNEL_3;
    else if (channel == 4) config.channel = Channel::CHANNEL_4;
    else if (channel == 5) config.channel = Channel::CHANNEL_5;
    else config.channel = Channel::CHANNEL_7;

    if (dataRate == 0) config.dataRate = DataRate::RATE_850KBPS;
    else config.dataRate = DataRate::RATE_6800KBPS;

    config.pulseFreq = PulseFrequency::FREQ_64MHZ;
    
    if (preambleLength <= 64) config.preambleLen = PreambleLength::LEN_64;
    else if (preambleLength <= 128) config.preambleLen = PreambleLength::LEN_128;
    else if (preambleLength <= 256) config.preambleLen = PreambleLength::LEN_256;
    else if (preambleLength <= 512) config.preambleLen = PreambleLength::LEN_512;
    else config.preambleLen = PreambleLength::LEN_1024;
    
    config.preaCode = (PreambleCode)preambleCode;

    DW1000Ng::applyConfiguration(config);

    interrupt_configuration_t int_config;
    int_config.interruptOnSent = true;
    int_config.interruptOnReceived = true;
    int_config.interruptOnReceiveFailed = true;
    int_config.interruptOnReceiveTimeout = true;
    int_config.interruptOnReceiveTimestampAvailable = false;
    int_config.interruptOnAutomaticAcknowledgeTrigger = false;

    DW1000Ng::applyInterruptConfiguration(int_config);
    return true;
}

bool UWB_DW1000::transmit(const uint8_t* data, uint16_t length) {
    DW1000Ng::forceTRxOff(); // Abort any ongoing RX before TX
    DW1000Ng::setTransmitData((byte*)data, length);
    DW1000Ng::startTransmit();
    return true;
}

bool UWB_DW1000::startReceive() {
    DW1000Ng::startReceive();
    return true;
}

bool UWB_DW1000::readReceivedData(uint8_t* buffer, uint16_t& length) {
    length = DW1000Ng::getReceivedDataLength();

    if (length > 0) {
        DW1000Ng::getReceivedData((byte*)buffer, length);

        // DW1000 does NOT continue RX automatically when RXAUTR is disabled.
        // Re-enable RX after payload has been copied.
        DW1000Ng::startReceive();

        return true;
    }

    // Recovery: if callback happened but length is invalid, still go back to RX.
    DW1000Ng::startReceive();
    return false;
}

float UWB_DW1000::getReceivePower() {
    return DW1000Ng::getReceivePower();
}

uint64_t UWB_DW1000::getTransmitTimestamp() {
    return DW1000Ng::getTransmitTimestamp();
}

uint64_t UWB_DW1000::getReceiveTimestamp() {
    return DW1000Ng::getReceiveTimestamp();
}

uint64_t UWB_DW1000::getSystemTimestamp() {
    return DW1000Ng::getSystemTimestamp();
}

void UWB_DW1000::onIRQ() {
    // If we handle the interrupt externally
    DW1000Ng::interruptServiceRoutine();
}

void UWB_DW1000::setCallbacks(UWB_Callback txDoneCb, UWB_Callback rxDoneCb) {
    txDoneCallback = txDoneCb;
    rxDoneCallback = rxDoneCb;
}

bool UWB_DW1000::enterDeepSleep() {
    DW1000Ng::deepSleep();
    return true;
}

bool UWB_DW1000::wakeUp() {
    DW1000Ng::spiWakeup();
    return true;
}

void UWB_DW1000::handleSent() {
    if (_instance && _instance->txDoneCallback) {
        _instance->txDoneCallback();
    }
}

void UWB_DW1000::handleReceived() {
    if (_instance && _instance->rxDoneCallback) {
        _instance->rxDoneCallback();
    }
}

void UWB_DW1000::handleError() {
}

void UWB_DW1000::handleReceiveFailed() {
    if (_instance) {
        DW1000Ng::startReceive();
    }
}

void UWB_DW1000::handleReceiveTimeout() {
    if (_instance) {
        DW1000Ng::startReceive();
    }
}
