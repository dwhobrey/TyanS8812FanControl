// kcsbmc.c
// This code is based on the Intel imb driver source code.
// So (c) belongs to Intel.
#include "driver.h"

#define ReadPortUchar(p,v) (v=(BYTE)(_inp(p)))
#define WritePortUchar(p,v) _outp(p,v)

// Default delays:
#define FORCE_DELAY  0xffffffff // If retryCount has this value, force a delay.

// This value is the number of times the DelayRetryFunc() just returns instead of
// doing a real OS delay. This value has been chosen by histogramming the BMC ready
// times on a Bear and choosing a value that covers the majority of hits.
#define NO_RETRY_DELAY_COUNT 0x100 

/*
* retryCount, number of times we've been called, not used here.
* delayTime, time to delay (in uSecs).
*/
VOID DelayRetryFunc(DWORD retryCount,DWORD delayTime) {
	LARGE_INTEGER  timeOutValue;
	if (retryCount < NO_RETRY_DELAY_COUNT && retryCount != FORCE_DELAY ) return;
	timeOutValue.u.HighPart = -1;
	timeOutValue.u.LowPart  = -((int)(delayTime * 10));   // 100ns units
	KeDelayExecutionThread(KernelMode, FALSE, &timeOutValue);
}

VOID DelayMicro(int micro) {
	DelayRetryFunc(FORCE_DELAY,micro);
}

VOID DelayMilli(int milli) {
	DelayRetryFunc(FORCE_DELAY,milli*1000);
}

//  Check to see if the SMS buffer has anything in it.   
//  Returns 1 if SMS_ATN flag set, otherwise 0.
int SmsBufferAvail(void) {
	BYTE status;
	ReadPortUchar( KCS_STATUS_REGISTER, status );
	return ((status & KCS_SMS_ATN) != 0) ? 1 : 0;
}

//  Get the STATE bits of the STATUS flag.  
BYTE GetKcsState(void) {
	BYTE status;
	ReadPortUchar( KCS_STATUS_REGISTER, status );
	return status & KCS_STATE_MASK;
}

//  Read the data port of the KCS interface.   
BYTE ReadKcsData(void) {
	BYTE data;
	ReadPortUchar( KCS_DATAOUT_REGISTER, data );
	return data;
}

//  Write to the data port of the KCS interface.  
void WriteKcsData(BYTE data) {
	WritePortUchar( KCS_DATAIN_REGISTER, data );
}

//  Write to the command port of the KCS interface.
void WriteKcsCmd(BYTE cmd) {
	WritePortUchar( KCS_COMMAND_REGISTER, cmd );
}

//  Determines if the Timeout Timer has timed out.
//  Returns 1 if the Timeout has expired, otherwise 0.
int ImbReqTimedOut(void *context) {
	return (((PDEVICE_CONTEXT)context)->timeOutFlag == 0) ? 1 : 0;
}

// Check to see if Input Buffer Full Flag is reset.   
// Returns 1 if IBF reset, otherwise 0.
int WaitOnIBF( void *context ) {
	DWORD retryCount = 0;
	BYTE status;
	for(;;) {
		ReadPortUchar( KCS_STATUS_REGISTER, status );	
		if( (status & KCS_IBF) == 0 ) break;
		DelayRetryFunc( retryCount++, KCS_READY_DELAY);
		if ( ImbReqTimedOut( context ) ) return 0;
	}
	return 1;
}

// Clears OBF flag if set 
// Returns 1.
int ClearOBF(void) {
    ReadKcsData();
	return 1;
}

// Get the OBF bit in the KCS interface.  
// Returns 1 if OBF set, otherwise 0.
int OBFset(void) {
	BYTE status;
	ReadPortUchar( KCS_STATUS_REGISTER, status );
	return ((status & KCS_OBF) == KCS_OBF)  ? 1 : 0;
}

// Check to see if OutPut Buffer Full Flag is set.  
// Returns TRUE if OBF set.
int WaitOnOBF( void *context ) {
	DWORD retryCount = 0;
	for(;;) {
        if( OBFset() ) break;
		DelayRetryFunc( retryCount++, KCS_READY_DELAY );
		if ( ImbReqTimedOut( context ) ) return 0;
	}
	return 1;
}

// Sends raw Imbp BMC message via kcs interface.
// It also returns the response (if expected) from the BMC.
// Returns STATUS_SUCCESS if the message was processed properly else error status. 
NTSTATUS SendBmcRequest(PBMC_REQUEST request, PBMC_REQUEST response, PVOID context) {
    NTSTATUS errStatus = STATUS_SUCCESS;
    BYTE invalidRespCount = 0;
	int	machineState = TRANSFER_INIT;
	int	i = 0;
	BYTE* transmitBuf = request->Data;
	int reqLength = (int)request->BlockLength;
	BYTE* receiveBuf = response->Data;
	response->BlockLength=0;

	DisableInts();

    for(;;) {
		// check for timeout
		if ( ImbReqTimedOut( context ) ) {
			// determine the cause
			if ( errStatus == STATUS_SUCCESS ) {
				if ( machineState > TRANSFER_END )
					errStatus = IMB_IF_RECEIVE_TIMEOUT;
				else
					errStatus = IMB_IF_SEND_TIMEOUT;
			}                       
			break;
			// check for successful termination
		} else if ( machineState == MACHINE_END ) {
			break;
			// check for too many bad responses
		} else if ( invalidRespCount > MAX_INVALID_RESPONSE_COUNT ) {
			errStatus = IMB_INVALID_IF_RESPONSE;
			break;
		}

		switch (machineState) {
		case  TRANSFER_INIT:
			i = 0;
			machineState = TRANSFER_START;
			WaitOnIBF( context );
			if ( ClearOBF() == 0)	{
				machineState = TRANSFER_ERROR;
				break;
			}
			// fall through
		case  TRANSFER_START:
			machineState = TRANSFER_NEXT;
			WritePortUchar( KCS_COMMAND_REGISTER, KCS_WRITE_START );
			WaitOnIBF( context );
            if( GetKcsState() != KCS_WRITE_STATE ) {
				machineState = TRANSFER_ERROR;
				break;
			}
			if ( ClearOBF() == 0)	{
				machineState = TRANSFER_ERROR;
				break;
			}
			// fall through, again
		case  TRANSFER_NEXT:
			// check for end of request, transition to the END state which will transfer
			// our last byte
			if ( i >= (reqLength - 1) ) {
				machineState = TRANSFER_END ;
				break;
			}
            WriteKcsData( transmitBuf[i++] );		
			WaitOnIBF( context );
            if( GetKcsState() != KCS_WRITE_STATE ) {
				machineState = TRANSFER_ERROR;
				break;
			}
			if ( ClearOBF() == 0)	{
				machineState = TRANSFER_ERROR;
				break;
			}
			break;
		case  TRANSFER_END:
			// Transfer the last byte of the request and
			// transition to Reading.
            WriteKcsCmd(KCS_WRITE_END);
			WaitOnIBF( context );
            if( GetKcsState() != KCS_WRITE_STATE ) {
				machineState = TRANSFER_ERROR;
				break;
			} 
			if ( ClearOBF() == 0)	{
				machineState = TRANSFER_ERROR;
				break;
			}
            WriteKcsData(transmitBuf[i++] );
			WaitOnIBF( context );
    		machineState = RECEIVE_START;
			// fall through
		case  RECEIVE_START:
			// end of request, now transition to receiving the response
            switch( GetKcsState() ) {
				case KCS_ERROR_STATE:
					machineState = TRANSFER_ERROR;
					break;
				case KCS_WRITE_STATE:
				case KCS_IDLE_STATE:
					// not ready with our response, hang out for a bit
					DelayRetryFunc( FORCE_DELAY, BMC_RESPONSE_DELAY);
					break;
				case KCS_READ_STATE:
					// ready with our response, setup to receive
					i = 0; 
					ZeroMemory( receiveBuf, KCS_MAX_REQUEST_SIZE );
					machineState = RECEIVE_INIT;
					break;
			}
			break;
		case  RECEIVE_INIT:
			// wait for a data byte to come available
            switch( GetKcsState() ) {
				case KCS_ERROR_STATE:
				case KCS_WRITE_STATE:
					machineState = TRANSFER_ERROR;
					break;
				case KCS_IDLE_STATE:
					// he's done sending us data
					machineState = RECEIVE_END;
					break;
				case KCS_READ_STATE:
                    if( OBFset() )
						machineState = RECEIVE_NEXT;
					else
						DelayRetryFunc(FORCE_DELAY, WAIT_1_MS);
					break;
				default:
					DelayRetryFunc(FORCE_DELAY, WAIT_1_MS);
					break;
			}
			break;
	   case  RECEIVE_NEXT:
			// Read the next data byte from the BMC.
			// check for overrun
			if (  i >= KCS_MAX_DATA_SIZE ) {
				errStatus = IMB_RESPONSE_DATA_OVERFLOW;
				machineState = TRANSFER_ERROR;
				break;
			}
			// get the data from the BMC.
            receiveBuf[i++] = ReadKcsData();
            WriteKcsData( KCS_READ );
			WaitOnIBF( context );
			machineState = RECEIVE_INIT2;
			break;									
		case  RECEIVE_INIT2:
			// wait for a data byte to come available
            switch( GetKcsState() ) {
			case KCS_ERROR_STATE:
			case KCS_WRITE_STATE:
				machineState = TRANSFER_ERROR;
				break;
			case KCS_IDLE_STATE:
				// he's done sending us data
				// added to support new IPMI 1.0 conformance requirements.                           
                if ( WaitOnOBF( context ) != 0) {
                    ClearOBF();
					machineState = RECEIVE_END;
				} else {
                    machineState =  TRANSFER_ERROR;
                }
				break;
			case KCS_READ_STATE:
				if ( WaitOnOBF( context ) != 0) {                            
					machineState = RECEIVE_NEXT;			
				} else
                    machineState =  TRANSFER_ERROR;
				break;
			}
			break;
	   case  RECEIVE_END:
#if 0
		   // check for size, appropriate response netfun, and command
		   if ( i < MIN_BMC_RESPONSE_SIZE ||
			    NETFN_OF(response->nfLn) != RESPONSE_NETFN(NETFN_OF(request->nfLn)) ||
				response->cmd != request->cmd ) {
			    // bad response, wait for a while and try sending it again
				DelayRetryFunc( FORCE_DELAY, BMC_RETRY_DELAY);
				machineState = TRANSFER_INIT;
				++invalidRespCount;
				break;
			}
#endif
		    // all done, return triumphant
			response->BlockLength = (BYTE)i;
			errStatus	 = STATUS_SUCCESS;
			machineState = MACHINE_END;
			break;
		case  TRANSFER_ERROR:
		default:
			// We've failed somehow, or maybe it was his fault.
			// Try again, as per comm spec. allow 60 ms. for controllers to recover
			// Can we force him into a known state?
			DelayRetryFunc( FORCE_DELAY, BMC_RETRY_DELAY );
			i = 0;
			machineState = TRANSFER_INIT;
			break;
       }
	}

	EnableInts();

	return errStatus;
}
