#pragma once
/*

This file currently holds all the hardware-specific "magic numbers" for controlling eNET-AIO Family devices.
Specifically, register offsets are provided as named constants (#defines), and bitmasks and values are #defined.

It should, someday, be replaced with a polymorphic HAL-style layer suitable for achieving the *goal* of
controlling different DAQ hardware from a single source file by swapping out the return values from this file.

Frex: replace all the #defines with an abstract base class fields, then derive one concrete singleton class per
      hardware type. Then:
          AIO_Device device = new AIO_eNET-AIO16-16F();
          device.Reset();
          device.DeviceID();
      or whatever
*/

constexpr __u8 maxSubmuxes = 4;
constexpr __u8 gainGroupsPerSubmux = 4;

constexpr __u8 NUM_DACS = 4;
constexpr int BAR_REGISTER = 1;

#define FIFO_SIZE 0x800  /* FIFO Almost Full IRQ Threshold value (0 < FAF <= 0xFFF */
#define SAMPLES_PER_FIFO_ENTRY   2
#define BYTES_PER_FIFO_ENTRY     (4 * SAMPLES_PER_FIFO_ENTRY)
#define BYTES_PER_TRANSFER       (FIFO_SIZE * BYTES_PER_FIFO_ENTRY)
#define SAMPLES_PER_TRANSFER     (FIFO_SIZE * SAMPLES_PER_FIFO_ENTRY)

/* Hardware registers */
#define ofsReset                0x00
    #define bmResetEverything       (1 << 0)
    #define bmResetAdc              (1 << 1)
    #define bmResetDac              (1 << 2)
    #define bmResetDio              (1 << 3)

#define ofsAdcRange             0x01 /* per-channel-group range is ofsAdcRange + channel group */
    #define bmGainX2                (1 << 1)
    #define bmGainX5                (1 << 2)
    #define bmBipolar               (1 << 0)
    #define bmUnipolar              (0 << 0)
    #define bmSingleEnded           (1 << 3)
    #define bmDifferential          (0 << 3)
    #define bmAdcRange_u10V         (bmUnipolar)
    #define bmAdcRange_b10V         (bmBipolar)
    #define bmAdcRange_u5V          (bmGainX2|bmUnipolar)
    #define bmAdcRange_b5V          (bmGainX2|bmBipolar)
    #define bmAdcRange_u2V          (bmGainX5|bmUnipolar)
    #define bmAdcRange_b2V          (bmGainX5|bmBipolar)
    #define bmAdcRange_u1V          (bmGainX2|bmGainX5|bmUnipolar)
    #define bmAdcRange_b1V          (bmGainX2|bmGainX5|bmBipolar)

#define ofsAdcCalibrationMode   0x11
    #define bmCalibrationOff        (0 << 0)
    #define bmCalibrationOn         (1 << 0)
    #define bmCalibrateVRef         (1 << 1)
    #define bmCalibrateGnd          (0 << 1)
    #define bmCalibrateBip          (1 << 2)
    #define bmCalibrateUnip         (0 << 2)

#define ofsAdcTriggerOptions    0x12
    #define bmAdcTriggerSoftware    (0b00 << 0)
    #define bmAdcTriggerTimer       (0b01 << 0)
    #define bmAdcTriggerExternal    (0b11 << 1)
    #define bmAdcTriggerTypeChannel (0 << 2)
    #define bmAdcTriggerTypeScan    (1 << 2)
    #define bmAdcTriggerEdgeRising  (0 << 3)
    #define bmAdcTriggerEdgeFalling (1 << 3)

#define ofsAdcStartChannel      0x13
#define ofsAdcStopChannel       0x14
#define ofsAdcOversamples       0x15
#define ofsAdcSoftwareStart     0x16
#define ofsAdcCrossTalkFeep     0x17
    #define bmAdcCrossTalk400k      (1 << 0)
    #define bmAdcCrossTalk500k      (0 << 0)

#define ofsAdcRateDivisor       0x18
#define ofsAdcDataFifo          0x1C
    #define bmAdcDataInvalid        (1 << 31)
    #define bmAdcDataChannelMask    (0x7F << 20)
    #define bmAdcDataGainMask       (0xF << 27)
    #define bmAdcDataMask           (0x0000FFFF)

#define ofsAdcFifoIrqThreshold  0x20
#define ofsAdcFifoCount         0x24
#define ofsIrqEnables           0x28
    #define bmIrqDmaDone            (1 << 0)
    #define bmIrqFifoAlmostFull     (1 << 1)
    #define bmIrqEvent              (1 << 3)

#define ofsIrqStatus_Clear      0x2C
//  #define bmIrqDmaDone            (1 << 0)
//  #define bmIrqFifoAlmostFull     (1 << 1)
//  #define bmIrqEvent              (1 << 3)

#define ofsDac                  0x30
#define ofsDacSpiBusy           0x30
    #define bmDacSpiBusy        (1 << 31)
#define ofsDacSleep             0x34

#define ofsDioDirections        0x3C // DIO Direction control
    #define bmDioInput              (1) // bit 0 is DIO#0, 1 is DIO#1 etc
    #define bmDioOutput             (0)
#define ofsDioSpiBusy           0x3C
    #define bmDioSpiBusy        (1 << 31) // SPI busy flag (overlaps with directions)

#define ofsDioOutputs           0x40
#define ofsDioInputs            0x44

#define ofsSubMuxSelect         0x9C    // used to override autodetect
    #define bmNoSubMux              0x03
    #define bmAIMUX64M              0x00
    #define bmAIMUX32               0x01


/* Flash is currently factory-use only and can out-of-warranty brick your unit */
#define ofsFlashAddress         0x70
#define ofsFlashData            0x74
#define ofsFlashErase           0x78

#define ofsFpgaID               0xA0
#define ofsFeatures             0xA4
#define ofsDeviceID             0xA8

#define ofsAdcBaseClock         0xAC
#define ofsAdcCalScale          0xC0
    #define ofsAdcCalScaleStride    8
#define ofsAdcCalOffset         0xC4
    #define ofsAdcCalOffsetStride   8

constexpr int BAR_DMA = 0;

#define ofsDmaAddr32            0x0
#define ofsDmaAddr64            0x4
#define ofsDmaSize              0x8
#define ofsDmaControl           0xc
    #define DmaStart            0x4
    #define DmaAbortClear       0x8
    #define DmaEnableSctrGthr   0x10


int widthFromOffset(int offset); // Defined in eNET-AIO16-16F.cpp
constexpr int RING_BUFFER_SLOTS = 255;

#define DMA_BUFF_SIZE (BYTES_PER_TRANSFER * RING_BUFFER_SLOTS)
