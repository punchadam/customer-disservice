#include "storage.h"
#include <Preferences.h>

SavedCustomer savedCustomers[MAX_CUSTOMERS];
u8 customerCount = 0;

static Preferences prefs;

// keys: "count", "n0"–"n3" (name), "a0"–"a3" (age), "h0"–"h3" (hat)

void loadCustomers() {
  prefs.begin("customers", true);  // read-only
  customerCount = prefs.getUChar("count", 0);
  if (customerCount > MAX_CUSTOMERS) customerCount = MAX_CUSTOMERS;

  for (u8 i = 0; i < MAX_CUSTOMERS; i++) {
    savedCustomers[i].valid = false;
  }

  for (u8 i = 0; i < customerCount; i++) {
    String nk = "n" + String(i);
    String ak = "a" + String(i);
    String hk = "h" + String(i);

    String name = prefs.getString(nk.c_str(), "");
    if (name.length() == 0) continue;

    name.toCharArray(savedCustomers[i].name, 9);
    savedCustomers[i].age = prefs.getUChar(ak.c_str(), 1);
    savedCustomers[i].hat = prefs.getUChar(hk.c_str(), 0);
    savedCustomers[i].valid = true;
  }
  prefs.end();
}

void saveCustomer(const Character& c) {
  // shift older entries down if we're at capacity
  if (customerCount >= MAX_CUSTOMERS) {
    for (u8 i = 0; i < MAX_CUSTOMERS - 1; i++) {
      savedCustomers[i] = savedCustomers[i + 1];
    }
    customerCount = MAX_CUSTOMERS - 1;
  }

  // add new customer at the end
  u8 idx = customerCount;
  strncpy(savedCustomers[idx].name, c.name.c_str(), 8);
  savedCustomers[idx].name[8] = '\0';
  savedCustomers[idx].age = c.age;
  savedCustomers[idx].hat = (u8)c.hat;
  savedCustomers[idx].valid = true;
  customerCount++;

  // write everything to flash
  prefs.begin("customers", false);  // read-write
  prefs.putUChar("count", customerCount);

  for (u8 i = 0; i < customerCount; i++) {
    String nk = "n" + String(i);
    String ak = "a" + String(i);
    String hk = "h" + String(i);

    prefs.putString(nk.c_str(), savedCustomers[i].name);
    prefs.putUChar(ak.c_str(), savedCustomers[i].age);
    prefs.putUChar(hk.c_str(), savedCustomers[i].hat);
  }
  prefs.end();
}

u32 deathCount = 0;

void loadDeathCount() {
  prefs.begin("stats", true);  // read-only
  deathCount = prefs.getULong("deaths", 0);
  prefs.end();
}

void incrementDeathCount() {
  deathCount++;
  prefs.begin("stats", false);  // read-write
  prefs.putULong("deaths", deathCount);
  prefs.end();
}