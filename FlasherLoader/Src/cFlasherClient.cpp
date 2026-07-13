//==================================================================================
//==================================================================================
// File: cFlasherClient.cpp
// Description: Utility for flashing QSPI Flash memory from a PC
//
// Copyright (c) 2024-2026 Dad Design.
//==================================================================================
//==================================================================================
#include "cFlasherClient.h"

//**********************************************************************************
// UsbCallbackFlasher - Reception of data read from USB serial link (COMxx)
//**********************************************************************************
// Data buffer for transmission blocks
Dad::cBuff   __FlasherClientDataBuff(sizeof(Bloc)*2);

void UsbCallbackFlasher(uint8_t* buf, uint32_t* len) {
    uint8_t* pBuff = buf;                                    // Pointer to input buffer
    for(uint32_t Index = *len; Index != 0; Index--) {        // Process all received bytes
    	__FlasherClientDataBuff.addData(*pBuff++);           // Add each byte to data buffer
    }
}

namespace DadPersistentStorage {

//**********************************************************************************
// class cFlasherClient
//**********************************************************************************

// Initialization
void cFlasherClient::Initialize(uint8_t *FlasherZoneAddr){
	m_pFirstFlashPage = (Page *) FlasherZoneAddr;
	m_ResultProcess = false;
	m_CurrentBloc = 0;

	// Initialize message start marker
    m_Msg.StartMarker[0] = 'B';
    m_Msg.StartMarker[1] = 'L';
    m_Msg.StartMarker[2] = 'O';
    m_Msg.StartMarker[3] = 'K';
    m_Msg.NumBloc = 0;
    m_TotalMemoryTransfered = 0;
    m_State = FlasherClientState::Reset;
    m_Result = FlasherClientResult::Wait;
}

// Main loop
void cFlasherClient::FlasherClientLoop(void){

	switch (m_State){
		case FlasherClientState::Reset :
			m_CurrentBloc = 0;
			m_State = FlasherClientState::Send;
			break;

		case FlasherClientState::Send :
			// Send request message to server
			__FlasherClientDataBuff.Clear();                    	// Clear reception buffer
			m_Msg.NumBloc = m_CurrentBloc;                      	// Set requested block number
			CDC_Transmit_FS((uint8_t *)&m_Msg, sizeof(m_Msg));      // Transmit request via USB
			m_tickSend = HAL_GetTick();
			m_State = FlasherClientState::WaitTrans;
			break;

		case FlasherClientState::WaitTrans :
			// Wait for block transmission with timeout
			if(__FlasherClientDataBuff.getNbData() >= sizeof(Bloc)){
				// Process the received block
				if( true == BlocProcess(m_CurrentBloc)){
					if(m_CurrentBloc == 0){
						m_TotalMemoryTransfered = 0;
					}
					m_Result = FlasherClientResult::Receve;
					m_CurrentBloc++;
					m_TotalMemoryTransfered += TAILLE_BLOC_TRANS;
					m_State = FlasherClientState::Send;
				}else{
					if(m_Result != FlasherClientResult::EndTrans){
						m_Result = FlasherClientResult::Error;
					}
					m_State = FlasherClientState::Reset;
				}
			}else if ((HAL_GetTick() - m_tickSend) >= VAL_MS_TIMEOUT){
		        // Timeout
				m_State = FlasherClientState::Reset;
				if(m_Result == FlasherClientResult::Receve){
					m_Result = FlasherClientResult::Error;
				}
		    }
			break;
	}
}

// Process one bloc
bool cFlasherClient::BlocProcess(uint16_t NumBloc){
	Bloc* pBloc = (Bloc*)__FlasherClientDataBuff.getBuffPtr();      // Cast buffer to block structure

	// Validate block structure and markers
	if((pBloc->StartMarker[0] != 'B') ||
	   (pBloc->StartMarker[1] != 'L') ||
	   (pBloc->StartMarker[2] != 'O') ||
	   (pBloc->StartMarker[3] != 'K') ||
	   (pBloc->EndMarker[0] != 'E') ||
	   (pBloc->EndMarker[1] != 'N') ||
	   (pBloc->EndMarker[2] != 'D') ||
	   (NumBloc != pBloc->NumBloc))
	{
		return false;                                        		// Invalid block structure
	}

	// Copy block data to page buffer and calculate CRC
	uint8_t* pPageBuff = m_PageBuff[NumBloc % NB_BLOC_TRANS]; 		// Target page buffer
	uint8_t* pBlocData = pBloc->Data;                        		// Source block data
	uint8_t  CalcCRC = 0;                                    		// CRC calculation variable

	for(uint16_t Index = 0; Index < TAILLE_BLOC_TRANS; Index++) {
		CalcCRC += *pBlocData;                               		// Accumulate CRC
		*pPageBuff++ = *pBlocData++;                         		// Copy data to page buffer
	}

	// Verify CRC checksum
	if(CalcCRC != pBloc->_CRC) {
		return false;                                        		// CRC mismatch
	}

	// Check if page is complete and needs to be flashed
	if(((NumBloc % NB_BLOC_TRANS) == (NB_BLOC_TRANS - 1)) || (pBloc->_EndTrans != 0)) {
		uint16_t NumPage = NumBloc / NB_BLOC_TRANS;          		// Calculate page number
		uint32_t adresseData = (uint32_t) &m_pFirstFlashPage[NumPage];
		//uint32_t adresseData = (uint32_t) &__FlasherData.Data[NumPage]; // Target QSPI address

		// Erase and write page to flash
		__Flash.EraseBlock4K(adresseData);
		__Flash.Write((uint8_t*)m_PageBuff, adresseData, QFLAH_SECTOR_SIZE);

		// Verify written data
		uint8_t* pQSPI = (uint8_t*) &m_pFirstFlashPage[NumPage];    // QSPI data pointer
		uint8_t* pPageBuff = (uint8_t*) m_PageBuff;                 // Source buffer pointer
		for(uint16_t Index = 0; Index < QFLAH_SECTOR_SIZE; Index++) {
			if(*pPageBuff++ != *pQSPI++) {                   		// Compare byte by byte
				return false;                                		// Verification failed
			}
		}
	}

	// Return continuation status
	if(pBloc->_EndTrans != 0) {
		m_Result = FlasherClientResult::EndTrans;
		return false;
	} else {
		return true;                                         // Continue processing
	}
}

} // namespace DadPersistentStorage

//***End of file**************************************************************





