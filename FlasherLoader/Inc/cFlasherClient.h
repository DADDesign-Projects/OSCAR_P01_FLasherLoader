//==================================================================================
//==================================================================================
// File: cFlasherClient.h
// Description: Utility for flashing QSPI Flash memory from a PC
//
// Copyright (c) 2024-2026 Dad Design.
//==================================================================================
//==================================================================================
#pragma once
#include "main.h"
#include "usbd_cdc_if.h"
#include "Buff.h"
#include "cFlasherStorage.h"

//**********************************************************************************
// Constants
//**********************************************************************************
constexpr uint32_t NbPagesFlasher = FLASHER_MEM_SIZE / QFLAH_SECTOR_SIZE;
typedef uint8_t Page[QFLAH_SECTOR_SIZE];

#define TAILLE_BLOC_TRANS    1024               					// Size of a single transmission block in bytes
constexpr uint32_t NB_BLOC_TRANS = QFLAH_SECTOR_SIZE/TAILLE_BLOC_TRANS; 	// Number of transmission blocks per QSPI page

constexpr uint32_t VAL_MS_TIMEOUT= 1000;        // Timeout value for block reception 1 second

// ---------------------------------------------------------------------------------
// Transmission block structure (server side)
struct Bloc {
    char        StartMarker[4];                 // Block start marker (e.g. "BLOC")
    uint16_t    NumBloc;                        // Block number
    uint8_t     _CRC;                           // Checksum for data integrity
    uint8_t     _EndTrans;                      // End-of-transmission flag
    uint8_t     Data[TAILLE_BLOC_TRANS];        // Block payload data
    char        EndMarker[3];                   // Block end marker (e.g. "END")
};

// ---------------------------------------------------------------------------------
// Transmission block request structure (client side)
struct MsgClient {
    char        StartMarker[4];                              // Block start marker (e.g. "BLOC" or "STOP")
    uint16_t    NumBloc;                                     // Number of the expected block
};

//**********************************************************************************
// UsbCallbackFlasher - Reception of data read from USB serial link (COMxx)
//**********************************************************************************

// Callback
extern "C" void UsbCallbackFlasher(uint8_t* buf, uint32_t* len);


namespace DadPersistentStorage {

//**********************************************************************************
// class cFlasherClient
//**********************************************************************************
enum class FlasherClientState{
	Reset,
	Send,
	WaitTrans,
};

enum class FlasherClientResult{
	Wait,
	Receve,
	EndTrans,
	Error,
	Dummy
};

class cFlasherClient {
public:
	// Constructor
	cFlasherClient() = default;

	// Initialization
	void Initialize(uint8_t *FlasherZoneAddr);

	// Main loop
	void FlasherClientLoop(void);

	// get state of class
	FlasherClientState 	getState(){
		return m_State;
	}

	// get result of class
	FlasherClientResult	getResult(){
		return m_Result;
	}

	// get State of class
	uint16_t getCurrentBloc(){
		return m_CurrentBloc;
	}

	// get total memory transfered	uint32_t getTotalMemoryTransfered(){
		return m_TotalMemoryTransfered;	}

protected :
	// Process one bloc
	bool BlocProcess(uint16_t NumBloc);

	Page* 				m_pFirstFlashPage;
	uint8_t				m_PageBuff[NB_BLOC_TRANS][TAILLE_BLOC_TRANS];
    bool        		m_ResultProcess = false;
    uint16_t    		m_CurrentBloc = 0;          		// Current block number
    uint32_t            m_TotalMemoryTransfered = 0;        // Total memory to transfered
    MsgClient   		m_Msg;								// Client message
    FlasherClientState 	m_State = FlasherClientState::Reset;
    FlasherClientResult m_Result = FlasherClientResult::Wait;
    uint32_t 			m_tickSend;

};


} // namespace DadPersistentStorage

//***End of file**************************************************************

