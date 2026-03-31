#pragma once

#include "types.h"

// Call once in setup() to load saved customers from flash
void loadCustomers();

// Save current player as a new customer (FIFO if full, oldest dropped)
void saveCustomer(const Character& c);

// death counter (persisted to flash)
extern u32 deathCount;

void loadDeathCount();
void incrementDeathCount();