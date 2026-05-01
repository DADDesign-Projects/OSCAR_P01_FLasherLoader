//==================================================================================
//==================================================================================
// File: PersistentDefine.h
// Description: Persistent storage configuration and memory layout definitions
// 
// Copyright (c) 2025 Dad Design.
//==================================================================================
//==================================================================================

#pragma once

//**********************************************************************************
// Includes
//**********************************************************************************
#include "main.h"
#include "FlashDefine.h"

//**********************************************************************************
// Flasher Storage Configuration
//**********************************************************************************

// Base address of external QSPI flash (16 MB total)
constexpr uint32_t FLASHER_ADDRESS  = FLASH_ADDRESS;      // Base QSPI Flash address
constexpr uint32_t FLASHER_MEM_SIZE = 16 * 1024 * 1024;   // 16 M

//**********************************************************************************
// Block Storage Manager Configuration
//**********************************************************************************

// Block Storage Manager region (upper 8 MB of flash)
constexpr uint32_t BLOCK_STORAGE_ADDRESS  = FLASH_ADDRESS + FLASHER_MEM_SIZE;   // After Flasher Storage
constexpr uint32_t BLOCK_STORAGE_MEM_SIZE = QFLAH_SECTOR_SIZE * 1024;           // 1024 blocs = 8 M

//**********************************************************************************
// Loader storage zone
//**********************************************************************************
constexpr uint32_t LOADER_STORAGE_ADDRESS  = BLOCK_STORAGE_ADDRESS + BLOCK_STORAGE_MEM_SIZE;  // After Bloc Storage
constexpr uint32_t LOADER_MEM_SIZE  = QFLAH_SECTOR_SIZE;  									  // One sector

//***End of file**************************************************************
