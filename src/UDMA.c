#include <stdint.h> // integer types and widths
#include <stdbool.h> // Boolean types
#include "inc/hw_ints.h"
#include "inc/hw_types.h" // common types
#include "inc/hw_memmap.h" // memory map for Tiva
#include "inc/hw_uart.h"
#include "driverlib/fpu.h" // access to FPU functions
#include "driverlib/gpio.h" // GPIO API
#include "driverlib/interrupt.h" // NVIC Interrupt Controller
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h" // system control driver
#include "driverlib/udma.h" // uDMA library
#include "math.h"

// Define 2 integer arrays of 1024 elements
#define MEM_BUFFER_SIZE 1024
//static uint32_t g_ui32SrcBuf[MEM_BUFFER_SIZE];
//static uint32_t g_ui32DestBuf[MEM_BUFFER_SIZE];

static float g_ui32SrcBuf[MEM_BUFFER_SIZE];
static float g_ui32DestBuf[MEM_BUFFER_SIZE];


// Define 2 variables dealing with the DMA errors and ISR errors
static uint32_t g_ui32DMAErrCount = 0; // DMA
static uint32_t g_ui32BadISR = 0;      // ISR

// Define a counter variable that will keep track of the number of transfers
static uint32_t g_ui32TransferCount = 0;

// Define an area used by the control table
#pragma DATA_ALIGN(pui8ControlTable, 1024); // It must be aligned to 1024 bytes (1K) in memory
uint8_t pui8ControlTable[1024];


//  Define a library error routine,
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

// Define the uDMA error handler
void uDMAErrorHandler()
{
    uint32_t ui32Status;

    ui32Status = uDMAErrorStatusGet();

    if(ui32Status)
    {
        uDMAErrorStatusClear();
        g_ui32DMAErrCount++;
    }
}

// Define the uDMA interrupt handle
void uDMAIntHandler()
{
    uint32_t ui32Mode;

    ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_SW);

    if(ui32Mode == UDMA_MODE_STOP) // Check whether the previous operation has completed
    {
        g_ui32TransferCount++; //  increment the counter for the completed operations

        uDMAChannelTransferSet(UDMA_CHANNEL_SW, UDMA_MODE_AUTO, g_ui32SrcBuf, g_ui32DestBuf, MEM_BUFFER_SIZE); // Configure uDMA for another transfer

        // Initiate another transfer
        uDMAChannelEnable(UDMA_CHANNEL_SW);
        uDMAChannelRequest(UDMA_CHANNEL_SW);
    }
    else
    {
        g_ui32BadISR++; //  increment the number of errors with the ISR
    }
}

// Initialize the uDMA software channel to perform a memory to memory uDMA transfer
void InitSWTransfer(void)
{
    //uint32_t ui32Idx;
    float ui32Idx;

    for(ui32Idx = 0; ui32Idx < MEM_BUFFER_SIZE; ui32Idx++) // Fill the source memory buffer with a simple incrementing pattern
    {
        float wave = ui32Idx/MEM_BUFFER_SIZE;
        wave = wave*(2*M_PI);
        wave = sin(wave);
        g_ui32SrcBuf[(int)ui32Idx] = wave;
        //g_ui32SrcBuf[ui32Idx] = ui32Idx;
    }

    IntEnable(INT_UDMA); // Enable interrupts from the uDMA software channel

    uDMAChannelAttributeDisable(UDMA_CHANNEL_SW, UDMA_ATTR_USEBURST | UDMA_ATTR_ALTSELECT | (UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK)); // Place the uDMA channel attributes in a known state.

    uDMAChannelControlSet(UDMA_CHANNEL_SW | UDMA_PRI_SELECT, UDMA_SIZE_32 | UDMA_SRC_INC_32 | UDMA_DST_INC_32 | UDMA_ARB_8); // Configure the control parameters for the SW channel

    uDMAChannelTransferSet(UDMA_CHANNEL_SW | UDMA_PRI_SELECT, UDMA_MODE_AUTO, g_ui32SrcBuf, g_ui32DestBuf, MEM_BUFFER_SIZE); // Set up the transfer parameters for the software channel

    uDMAChannelEnable(UDMA_CHANNEL_SW); // The channel must be enabled
    uDMAChannelRequest(UDMA_CHANNEL_SW); // a request must be issued

}


int main(void)
{
    FPULazyStackingEnable(); // Enable FPU lazy stacking

    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ); // Set the frequency to 50 MHz

    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA); // Enable the uDMA module

    // Enable sleep of the uDMA module
    SysCtlPeripheralClockGating(true); // Set peripheral clock gating to true
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UDMA); // Allow the uDMA module to sleep

    IntEnable(INT_UDMAERR); // Enable an interrupt for errors on the uDMA

    uDMAEnable(); // Enable the uDMA module

    uDMAControlBaseSet(pui8ControlTable); // Set the base address of the uDMA control table

    InitSWTransfer(); // Initiate a transfer

    while(1) // Infinite loop
    {

    }

}
