#pragma once
#include <Arduino.h>

class ClickerMode {
public:
    virtual void        start()                         = 0;
    virtual void        stop()                          = 0;
    virtual void        poll(uint32_t now)              = 0;
    virtual bool        running()               const   = 0;
    virtual const char* name()                  const   = 0;
    virtual void        setParam(const char* key,
                                 const char* val)       = 0;
    virtual String      statusJson()            const   = 0;

    // Phase 6: NVS persistence. Default no-ops so modes can opt in incrementally.
    virtual void        loadFromNVS()                   {}
    virtual void        saveToNVS()                     {}

    virtual ~ClickerMode() {}
};
