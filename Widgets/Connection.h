#pragma once

#include "Shared.h"

#include "CasparDevice.h"

class Connection
{
    public:
        explicit Connection();
        ~Connection();

        static Connection& getInstance();

        CasparDevice& getDevice();

    private:
        CasparDevice* device;
};