#pragma once

#include "types.h"

// Call once in setup() to load saved customers from flash
void loadCustomers();

// Save current player as a new customer (FIFO if full, oldest dropped)
void saveCustomer(const Character& c);