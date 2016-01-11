// kcsbmc.h
// This code is based on the Intel imb driver source code.
// So (c) belongs to Intel.

#define IMB_IF_SEND_TIMEOUT ((NTSTATUS)0xE0070006L)
#define IMB_IF_RECEIVE_TIMEOUT ((NTSTATUS)0xE0040007L)
#define IMB_INVALID_IF_RESPONSE ((NTSTATUS)0xE004000AL)
#define IMB_RESPONSE_DATA_OVERFLOW ((NTSTATUS)0xE004000CL)

#define KCS_READY_DELAY     WAIT_1_MS
#define BMC_RESPONSE_DELAY	WAIT_5_MS	// how long to wait for a response after sending req
#define BMC_RETRY_DELAY		(60 * WAIT_1_MS)	// how long to wait after an error before retrying

// Keyboard Controller Style Interface addresses and defines
#define KCS_COMMAND_REGISTER	0x0CAC
#define KCS_STATUS_REGISTER		0x0CAC
#define KCS_DATAIN_REGISTER		0x0CA8
#define KCS_DATAOUT_REGISTER	0x0CA8

#define MAX_INVALID_RESPONSE_COUNT	2 // number of BMC request send retries 

// Status register bit flags
#define KCS_STATE_MASK		0xc0
#define KCS_IDLE_STATE		0x00
#define KCS_READ_STATE		0x40
#define KCS_WRITE_STATE		0x80
#define KCS_ERROR_STATE		0xc0

#define KCS_IBF				0x02
#define KCS_OBF				0x01

#define KCS_SMS_ATN			0x04

// State machine states
#define TRANSFER_INIT		1
#define TRANSFER_START		2
#define TRANSFER_NEXT		3
#define TRANSFER_END		4	// send last byte of request
#define RECEIVE_START		5	// wait for BMC to transition to read state
#define RECEIVE_INIT		6	// wait for first data byte to become available
#define RECEIVE_NEXT		7	// collect data byte and ask for more
#define RECEIVE_INIT2		8	// wait for next data byte to become available
#define RECEIVE_END			9
#define MACHINE_END			10
#define TRANSFER_ERROR		0

// KCS commands
#define KCS_WRITE_START		0x61
#define KCS_WRITE_END		0x62
#define KCS_READ			0x68

#define KCS_MAX_DATA_SIZE		33	// As per spec.
#define KCS_MIN_REQUEST_SIZE    1
#define KCS_MAX_REQUEST_SIZE    (KCS_MIN_REQUEST_SIZE + KCS_MAX_DATA_SIZE)

// This structure represents a request directed to the BMC. 
// It is meant to be overlayed on a char array of size KCS_MAX_REQUEST_SIZE.
typedef struct _BMC_REQUEST {
	BYTE    BlockLength;
	BYTE    Data[KCS_MAX_DATA_SIZE];
} BMC_REQUEST, *PBMC_REQUEST;

extern NTSTATUS SendBmcRequest(PBMC_REQUEST request,PBMC_REQUEST response, PVOID context);
