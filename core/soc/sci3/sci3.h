#pragma once

#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>

#include "core/soc/memory/regions/io.h"
#include "core/utils/event_handler.h"

#define SCI3_ADDR_SMR 0xFF98
#define SCI3_ADDR_BRR 0xFF99
#define SCI3_ADDR_SCR 0xFF9A
#define SCI3_ADDR_TDR 0xFF9B
#define SCI3_ADDR_SSR 0xFF9C
#define SCI3_ADDR_RDR 0xFF9D
#define SCI3_ADDR_IRCR 0xFFA7

union SMR_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t CKS : 2;
        uint8_t MP : 1;
        uint8_t STOP : 1;
        uint8_t PM : 1;
        uint8_t PE : 1;
        uint8_t CHR : 1;
        uint8_t COM : 1;
    };
};

union SSR_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t MPBT : 1;
        uint8_t MPB : 1;
        uint8_t TEND : 1;
        uint8_t PER : 1;
        uint8_t FER : 1;
        uint8_t OER : 1;
        uint8_t RDRF : 1;
        uint8_t TDRE : 1;
    };
};

union SCR_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t CK : 2;
        uint8_t TEIE : 1;
        uint8_t MPIE : 1;
        uint8_t RE : 1;
        uint8_t TE : 1;
        uint8_t RIE : 1;
        uint8_t TIE : 1;
    };
};

union IRCR_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t : 4;
        uint8_t IRCKS : 3;
        uint8_t IRE : 1;
    };
};


class SCI3
{
public:
    void RegisterIOHandlers(const std::shared_ptr<IO>& io);

    void Cycle(uint8_t cycles);

    void OnTransmitIR(const EventHandlerCallback<uint8_t>& callback);
    void ReceiveIR(uint8_t data);
    void SaveEmulatorState(std::ostream& stream);
    bool LoadEmulatorState(std::istream& stream);

    SMR_t SMR = {};
    SSR_t SSR = {};
    SCR_t SCR = {};

    IRCR_t IRCR = {};

    uint8_t BRR = 0;

    uint8_t TDR = 0;
    uint8_t RDR = 0;
private:
    uint16_t sci3_cycles = 0;

    EventHandler<uint8_t> on_transmit_ir;

    std::queue<uint8_t> receive_queue;
    std::mutex receive_mutex;
};
