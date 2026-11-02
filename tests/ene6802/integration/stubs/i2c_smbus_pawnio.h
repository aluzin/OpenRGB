// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "i2c_smbus.h"
// No PawnIO library, driver, bus discovery or hardware backend is linked.
class i2c_smbus_pawnio : public i2c_smbus_interface {};
