//==================================================================================
//==================================================================================
// File    : cLoaderTarget.hpp
// Description : Manages persistent ELF target index in flash memory
//               using a wear-leveling circular log across an 8K page.
//
// Flash page layout:
//
//  ┌─────────┬────────┬────────┬─────┬──────────┐
//  │ Slot 0  │ Slot 1 │ Slot 2 │ ... │ Slot 511 │
//  │ (guard) │ valid  │ valid  │     │  0xFFFF  │ <- last valid = active
//  └─────────┴────────┴────────┴─────┴──────────┘
//
//  Slot 0 = guard slot, written once after erase to confirm
//           the page is properly initialized. Contains two
//           magic numbers for extra integrity confidence.
//  Slots 1..SLOT_COUNT-1 = circular wear-leveling log.
//  Erase only occurs when all data slots are consumed.
//
//  Copyright (c) 2025-2026 Dad Design.
//==================================================================================
//==================================================================================
#pragma once

#include <stdint.h>
#include "Sections.h"
#include "PersistentDefine.h"
#include "cFlasherStorage.h"

// -------------------------------------------------------------------
// Persistent Target — reserved in QFLASH_LOADER flash section
QFLASH_LOADER uint8_t __LoaderInfo[QFLAH_SECTOR_SIZE];

class cLoaderTarget {

public:
    // ---------------------------------------------------------------
    // Constructor
    // ---------------------------------------------------------------
    cLoaderTarget() {
        m_pLoaderInfo = (Slot*)__LoaderInfo;
    }

    // ---------------------------------------------------------------
    // Initialize flash page.
    // Must be called once at startup.
    // If the guard slot (slot 0) is invalid, the entire page is
    // erased and the guard is written to confirm initialization.
    // ---------------------------------------------------------------
    void init() {
        if (!isGuardValid()) {
            eraseAndWriteGuard();
        }
    }

    // ---------------------------------------------------------------
    // Save the selected ELF file index into flash.
    // Writes sequentially to minimize erase cycles.
    // Erases the page (and rewrites guard) only when all slots are
    // consumed.
    //
    // __Param__ : indexELF -> Index of the ELF file to execute
    // ---------------------------------------------------------------
    void save(uint16_t indexELF) {
        Slot slot;
        slot.magic1   = MAGIC1;
        slot.magic2   = MAGIC2;
        slot.indexELF = indexELF;
        slot.crc      = computeCRC(slot);

        uint16_t activeSlot = findActiveSlot();
        uint16_t nextSlot;

        if (activeSlot == SLOT_COUNT) {
            // All data slots consumed -> erase, reinit guard, start at slot 1
            nextSlot = FIRST_DATA_SLOT;
            eraseAndWriteGuard();
        } else {
            nextSlot = activeSlot + 1;
            if (nextSlot >= SLOT_COUNT) {
                // Page full -> erase, reinit guard, wrap around
                nextSlot = FIRST_DATA_SLOT;
                eraseAndWriteGuard();
            }
        }

        uint32_t targetAddr = (uint32_t)__LoaderInfo + (nextSlot * sizeof(Slot));
        __Flash.Write(reinterpret_cast<uint8_t*>(&slot), targetAddr, sizeof(Slot));
    }

    // ---------------------------------------------------------------
    // Read the active ELF file index from flash.
    //
    // Return : Valid ELF index, or DIR_FILE_COUNT+1 if no valid entry.
    // ---------------------------------------------------------------
    uint16_t load() const {
        uint16_t activeSlot = findActiveSlot();
        if (activeSlot == SLOT_COUNT) {
            return DIR_FILE_COUNT + 1;  // No valid entry found
        }
        return m_pLoaderInfo[activeSlot].indexELF;
    }

    // ---------------------------------------------------------------
    // Full erase + guard rewrite (hard reset).
    // ---------------------------------------------------------------
    void erase() {
        eraseAndWriteGuard();
    }

    // ---------------------------------------------------------------
    // Return the number of remaining free data slots before next erase.
    // ---------------------------------------------------------------
    uint16_t remainingSlots() const {
        uint16_t activeSlot = findActiveSlot();
        if (activeSlot == SLOT_COUNT) {
            return DATA_SLOT_COUNT;     // All data slots free
        }
        return SLOT_COUNT - 1 - activeSlot;
    }

// -------------------------------------------------------------------
private:

    // ---------------------------------------------------------------
    // Internal slot layout (8 bytes, naturally aligned)
    //
    //  Offset  Field     Size  Description
    //  0       magic1    2B    First magic number  (0xAB51)
    //  2       magic2    2B    Second magic number (0x51AB)  <- guard uses both
    //  4       indexELF  2B    ELF file index
    //  6       crc       2B    CRC16-CCITT over magic1+magic2+indexELF
    // ---------------------------------------------------------------
    struct Slot {
        uint16_t magic1;    // 0xAB51
        uint16_t magic2;    // 0x51AB  (guard slot: double validation)
        uint16_t indexELF;  // Selected ELF file index
        uint16_t crc;       // CRC16-CCITT over magic1 + magic2 + indexELF
    };

    // ---------------------------------------------------------------
    // Constants
    // ---------------------------------------------------------------
    static constexpr uint16_t MAGIC1           = 0xAB51U;
    static constexpr uint16_t MAGIC2           = 0x51ABU;
    static constexpr uint16_t GUARD_SLOT       = 0U;
    static constexpr uint16_t FIRST_DATA_SLOT  = 1U;
    static constexpr uint16_t SLOT_COUNT       = QFLAH_SECTOR_SIZE / sizeof(Slot);
    static constexpr uint16_t DATA_SLOT_COUNT  = SLOT_COUNT - 1U; // Slot 0 reserved

    // ---------------------------------------------------------------
    // Members
    // ---------------------------------------------------------------
    Slot* m_pLoaderInfo;

    // ---------------------------------------------------------------
    // Erase the 8K flash page and write the guard slot (slot 0).
    // ---------------------------------------------------------------
    void eraseAndWriteGuard() {
        __Flash.EraseBlock4K((uint32_t)__LoaderInfo);

        Slot guard;
        guard.magic1   = MAGIC1;
        guard.magic2   = MAGIC2;
        guard.indexELF = 0xFFFF;   // Unused, keep erased-like value
        guard.crc      = computeCRC(guard);

        __Flash.Write(reinterpret_cast<uint8_t*>(&guard),
                      (uint32_t)__LoaderInfo + (GUARD_SLOT * sizeof(Slot)),
                      sizeof(Slot));
    }

    // ---------------------------------------------------------------
    // Check that slot 0 (guard) is valid.
    // Both magic numbers AND CRC must match.
    // ---------------------------------------------------------------
    bool isGuardValid() const {
        const Slot& g = m_pLoaderInfo[GUARD_SLOT];
        if (g.magic1 != MAGIC1 || g.magic2 != MAGIC2) {
            return false;
        }
        return computeCRC(g) == g.crc;
    }

    // ---------------------------------------------------------------
    // Validate a data slot (slots 1..SLOT_COUNT-1).
    // Both magic numbers AND CRC must match.
    // ---------------------------------------------------------------
    bool isValid(uint16_t index) const {
        const Slot& s = m_pLoaderInfo[index];
        if (s.magic1 != MAGIC1 || s.magic2 != MAGIC2) {
            return false;
        }
        return computeCRC(s) == s.crc;
    }

    // ---------------------------------------------------------------
    // Scan data slots (1..SLOT_COUNT-1) and return the index of the
    // LAST valid slot.
    // Returns SLOT_COUNT if no valid data slot is found.
    // ---------------------------------------------------------------
    uint16_t findActiveSlot() const {
        uint16_t lastValid = SLOT_COUNT;    // Sentinel: no valid slot
        for (uint16_t i = FIRST_DATA_SLOT; i < SLOT_COUNT; i++) {
            if (isValid(i)) {
                lastValid = i;
            }
        }
        return lastValid;
    }

    // ---------------------------------------------------------------
    // CRC16-CCITT over magic1 + magic2 + indexELF (6 bytes).
    // The crc field itself is excluded from computation.
    // ---------------------------------------------------------------
    static uint16_t computeCRC(const Slot& slot) {
        uint16_t crc  = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&slot);
        uint16_t len  = sizeof(Slot) - sizeof(slot.crc); // Exclude CRC field

        while (len--) {
            crc ^= static_cast<uint16_t>(*data++) << 8;
            for (uint8_t i = 0; i < 8; i++) {
                crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
            }
        }
        return crc;
    }
};
//***End of file**************************************************************
