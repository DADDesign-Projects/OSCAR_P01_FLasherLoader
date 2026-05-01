#pragma once
//************************************************************************
// Buff.h
//
// Memory buffer management class
// This class implements a simple FIFO-style memory buffer for storing
// bytes, providing basic operations to add data, clear the buffer, and
// retrieve its pointer and data count.
//
// Copyright (c) 2024-2025 Dad Design
// Licensed under the MIT License. See LICENSE file for details.
//************************************************************************
#include "stdint.h"

namespace Dad {
    class cBuff {
    public:
    	//------------------------------------------------------------------------
        // Constructor
        // Allocates the buffer with the specified size (TailleFIFO) in bytes
        cBuff(uint16_t TailleFIFO) {
            m_pStartBuff = new uint8_t[TailleFIFO];       // Allocate buffer memory
            m_pEndBuff   = &m_pStartBuff[TailleFIFO];     // Set pointer to end of buffer
            Clear();                                      // Initialize buffer state
        };

        //------------------------------------------------------------------------
        // Destructor
        // Frees the allocated buffer and resets internal pointers
        ~cBuff() {
            delete [] m_pStartBuff; m_pStartBuff = nullptr;
            m_pEndBuff = nullptr;
            Clear();
        };

        //------------------------------------------------------------------------
        // Add a byte to the buffer
        // Returns true if successful, false if buffer is full
        inline bool addData(uint8_t Data) {
            if (m_pNextData < m_pEndBuff) {
                *m_pNextData++ = Data; // Store byte and increment pointer
                m_NbData++;            // Increase stored data count
                return true;
            } else {
                return false;          // No space left
            }
        }

        //------------------------------------------------------------------------
        // Clear the buffer content
        // Resets the next write pointer and data count
        inline void Clear() {
            m_pNextData = m_pStartBuff;
            m_NbData = 0;
        }
        
        //------------------------------------------------------------------------
        // Get pointer to the start of the buffer
        inline uint8_t* getBuffPtr() {
            return m_pStartBuff;
        }
        
        //------------------------------------------------------------------------
        // Get the number of bytes currently stored in the buffer
        inline uint16_t getNbData() {
            return m_NbData;
        }

    protected:
        //------------------------------------------------------------------------
        // Data members
        uint8_t* m_pStartBuff = nullptr; // Pointer to the start of the buffer memory
        uint8_t* m_pEndBuff   = nullptr; // Pointer to the end of the buffer memory
        uint8_t* m_pNextData  = nullptr; // Pointer to the next free slot for data
        uint16_t m_NbData = 0;           // Number of bytes stored in the buffer
    };
}
