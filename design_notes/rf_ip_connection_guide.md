# Technical Reference: Integrating OFDM FPGA Designs with Zynq UltraScale+ RFSoC IP (ZCU111)

### 1. Hardware Environment and Board Specifications
This design utilizes the Xilinx ZCU111 Evaluation Kit as the reference platform. While internal bring-up documentation may refer to "48DR" procedures, the target silicon is the Zynq UltraScale+ ZU28DR (Gen 1 RFSoC). The hardware environment is optimized for high-dynamic-range applications using the following physical configuration:

*   **RFSoC Device:** XCZU28DR-FFVG1517-2-E.
*   **RF Signal Path:** 8T8R (8-Transmit, 8-Receive) architecture accessible via SMA connectors.
*   **Analog Interface:** XM500 Balun Card. This card is mandatory for these designs to provide the necessary Low Frequency (LF) balun connections for the ADC/DAC tiles.
*   **Processing Subsystem (PS):** Integrated Quad-core Arm Cortex-A53, utilized for system orchestration and clock synthesis management.

### 2. RF Clocking Architecture and Frequency Plan
Achieving phase-coherent, low-jitter sampling requires a precise hierarchical clock distribution tree. The ZCU111 utilizes an LMK04208 clock jitter cleaner to drive three LMX2594 wideband PLLs in parallel.

**Clock Distribution Details:**
*   **Reference Source:** A 25 MHz TCXO provides the primary reference to the LMK04208.
*   **Primary Distribution:** The LMK04208 generates the REFIN_2594 and SYNC signals for the LMX2594 PLLs, ensuring phase alignment across the converters.
*   **VCXO Frequency:** 122.88 MHz.
*   **Target Sample Rate (Fs):** 1.47456 GHz for both ADC and DAC tiles.
*   **Control Interface:** All clock registers are programmed via an I2C-to-SPI bridge managed by the standalone software application.

**Fabric Frequency Synthesis:**
The AXI4-Stream fabric clock (184.32 MHz) is derived directly from the sampling rate and the decimation/interpolation ratio:
`1474.56 MHz (Fs) / 8 (Decimation/Interpolation) = 184.32 MHz`.

### 3. RF Data Converter (RFDC) IP Core Configuration
The `usp_rf_data_converter` IP must be configured to align with the physical RF path and the decimation/interpolation requirements of the OFDM sub-system.

**DAC Configuration (Tile 229, Channel 3 / Board Alias: Tile 1, Ch 3):**
| Parameter | Setting | Reason/Constraint |
| :--- | :--- | :--- |
| Analog Output Data | Real | Required for LF balun interface |
| Interpolation Mode | 8x | Sets fabric clock to 184.32 MHz |
| Samples per Cycle | 1 | Direct 16-bit sample mapping |
| Mixer Type | Bypassed | Digital up-conversion handled in fabric |
| Nyquist Zone | Zone 1 | Prevents imaging for 10MHz baseband |

**ADC Configuration (Tile 224, Channel 0 / Board Alias: Tile 0, Ch 0):**
| Parameter | Setting | Reason/Constraint |
| :--- | :--- | :--- |
| Digital Output Data | Real | Required for LF balun interface |
| Decimation Mode | 8x | Matches TX interpolation ratio |
| Samples per Cycle | 1 | Ensures simplified OFDM alignment |
| Mixer Type | Bypassed | Baseband processing in FPGA logic |
| Link Coupling | AC | Standard for XM500 LF path |
| Nyquist Zone | Zone 1 | Optimized for baseband signal capture |

### 4. AXI4-Stream Data Path and Logic Interface
The logic interface bridges the high-speed RF converters with the FPGA fabric. The reference design (tested in `c:\rfsoc\ex_des\zcu111\v4\`) uses the following IP:

*   **TX Path:** A `DDS Compiler 6.0` generates a 10 MHz continuous sine wave. This serves as the reference signal for loopback verification.
*   **RX Path:** Data from the ADC Tile 224 is routed to a `System ILA` for real-time signal integrity analysis in the Vivado Hardware Manager.
*   **Data Width:** The AXI4-Stream TDATA width is 16 bits per sample.
*   **Timing:** The entire data path must be constrained to the 184.32 MHz domain to prevent metastability and timing violations during data transfer to/from the RFDC.

### 5. Physical Port Mapping: External RF Loopback Path
To validate the integrated design, an External RF Loopback Path must be established on the XM500 card using high-quality coaxial cabling.

*   **Source (TX):** DAC Tile 229, Channel 3 (Labeled as DAC Tile 1, Ch 3 on board).
*   **Sink (RX):** ADC Tile 224, Channel 0 (Labeled as ADC Tile 0, Ch 0 on board).
*   **Connection:** Connect the SMA output of the DAC tile directly to the SMA input of the ADC tile via the LF balun ports.

### 6. Step-by-Step Hardware Bring-up Procedure
1.  **Switch Configuration:** Set SW6 to JTAG Mode (on, on, on, on) for development or SD Card Mode (on, off, off, off) for field testing.
2.  **Hardware Connectivity:** Connect USB for JTAG and UART. Open TeraTerm (per UG1036) to monitor the Processor System UART.
3.  **Initialization Sequence:**
    *   Apply power to the ZCU111.
    *   Initialize the Board Support Package (BSP) and execute the clock configuration routine in Vitis to lock the LMK/LMX chain at 1.47456 GHz.
    *   Issue the Data Converter Master Reset via the RFDC driver API.
4.  **Verification:** Monitor the UART output. Ensure the RFDC initialization reaches "Power-on Sequence Step 15." This confirms the tiles are fully operational and the state machines are locked.

### 7. Software Architecture and Vitis Project Management
The software layer is developed using the Vitis Unified Software Platform (Version 2020.2 or 2022.2).

*   **Hardware Platform:** Export the `design_1_wrapper.xsa` from Vivado, ensuring the bitstream is included.
*   **Workspace Configuration:** Create a Platform Project targeting the `psu_cortexa53_0` processor using the `standalone` OS.
*   **Application Setup:** Import the provided source files from the `src` directory. The application is responsible for the I2C-to-SPI bridge communication to initialize the RF clock tree before the RFDC IP is released from reset.

### 8. Integration Guide: Custom OFDM IP with RFDC
To integrate a custom OFDM IP, the developer must adhere to the following architectural constraints to ensure interface compatibility:

*   **TX Integration:** Replace the `DDS Compiler 6.0` with the custom OFDM TX core. The core must output 16-bit real data over an AXI4-Stream interface. Because "Samples per Cycle" is set to 1, the `m_axis_tdata` width must be exactly 16 bits.
*   **RX Integration:** Replace the `System ILA` with the custom OFDM RX core. The receiver must be designed to sink data at the full 184.32 MHz fabric rate.
*   **Clock Domain Management:** The custom OFDM logic must reside entirely within the `clk_adc0` or `clk_dac1` clock domains (184.32 MHz). Do not use the 100 MHz AXI-Lite clock for data processing logic to avoid complex Clock Domain Crossing (CDC) issues.
*   **Register Access:** Custom IP control and status registers should be mapped to the AXI4-Lite interconnect (running at 100.0 MHz) for management by the ARM Cortex-A53.