/*
    Copyright (C) 2020 Martti Ylioja
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <cstdint>

namespace PerformanceTimer {

    int64_t get_timestamp();

    //  elapsed time in microseconds
    int64_t get_elapsed_time(int64_t start);

}
