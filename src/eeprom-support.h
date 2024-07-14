// Define the addresses where you want to store your uint64_t variables
bool has_credentials() {
  return EEPROM.read(0) == 0x42 /* credentials marker */;
}
// Define addresses for storing numbers in EEPROM
const int ADDRESS_NUMBER1 = 1; // Address for the first number
const int ADDRESS_NUMBER2 = 11; // Address for the second number

// Function to save a number as a string to EEPROM
void saveNumberToEEPROM(long number, int address) {
    if (!has_credentials()) EEPROM.write(0, 0x42);
    // Convert the number to a string
    char buffer[9]; // 8 digits + null terminator
    sprintf(buffer, "%ld", number);
  
    // Write each character of the string to EEPROM
    for (int i = 0; i < 8; i++) {
        EEPROM.write(address + i, buffer[i]);
    }
    // Null-terminate the string in EEPROM
    EEPROM.write(address + 8, '\0');
    EEPROM.commit();
}

// Function to load a number as a string from EEPROM
long loadNumberFromEEPROM(int address) {
  if (!has_credentials()) return 0;
    // Read each character from EEPROM and reconstruct the string
    char buffer[9];
    for (int i = 0; i < 8; i++) {
        buffer[i] = EEPROM.read(address + i);
    }
    // Null-terminate the string
    buffer[8] = '\0';
  
    // Convert the string back to a number
    long number = atol(buffer);
    return number;
}
