//==================================================================================
//==================================================================================
// File: FlashDefine.h
// Description: Flash memory definitions and configuration for W25Q128 in dual mode
// 
// Copyright (c) 2025 Dad Design.
//==================================================================================
//==================================================================================

#pragma once

//**********************************************************************************
// FLASH - W25Q128 in double mode
//**********************************************************************************

#include "W25Q128.h"

// External flash memory instance
extern DadDrivers::cW25Q128 __Flash;

// Flash memory configuration constants
constexpr uint32_t QFLASH_SIZE       = 16 * 2 * 1024 * 1034;   // 16MB * 2 (dual mode)
constexpr uint32_t QFLAH_SECTOR_SIZE = 8 * 1024;               // 8KB per sector
constexpr bool     DOUBLE_MODE       = true;                   // Enable double mode

// Flash memory base address
#define FLASH_ADDRESS 0x90000000

//***End of file**************************************************************
