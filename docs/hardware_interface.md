# Hardware interface and voltage assumptions

This table describes the documented reference assumptions used by the simulated Hardware Studio. It is not a wiring instruction and does not represent measured hardware access.

| Device | Power assumptions | Grounding | Voltage domains | Level shifting | Connectors | Host communication | JTAG signal ownership | Device-specific assumptions | Safety precautions |
|---|---|---|---|---|---|---|---|---|---|
| Arduino Nano RP2040 Connect | Arduino-documented VIN range 5–18 V; board regulators provide the MCU domain | Common target/host reference required for any real bench | RP2040/GPIO modeled as 3.3 V | Do not drive GPIO above the documented 3.3 V domain; use a translator for other rails | USB Micro-B and Nano headers; RP2040 SWD pads are not a JTAG header | USB device; simulated here | SWD belongs to the RP2040 debug pads; J-Link wiring is not assumed | Pinout and VIN assumptions come from Arduino’s cited pinout/datasheet | Current-limit supplies, verify pinout/polarity, and never connect an unverified rail |
| Digilent Basys 3 | USB-JTAG/UART or 5 V external input per the reference manual | Common reference required for external signals | FPGA user I/O modeled as 3.3 V LVCMOS | Translate any attached non-3.3 V peripheral; USB-UART is not a JTAG substitute | Micro-AB J4 for USB-JTAG/UART; 6-pin J5 JTAG header | USB-JTAG/UART; simulated here | Board JTAG chain is owned by the FPGA configuration/debug path | Artix-7 35T metadata is descriptive only | Do not attach an external source without checking board power and signal levels |
| SEGGER J-Link EDU Mini | USB-powered; does not power the target | VTref senses target reference; target ground must be connected in a real setup | Target interface VIF 1.8/3.3 V | VTref selects probe I/O reference; external translation may still be required | 9-pin 0.05-inch target connector | USB 2.0 Full-Speed; simulated here | Probe owns TCK/TMS/TDI/TDO, reset, and VTref debug transactions; it is not a compute board | JTAG/SWD probe role follows SEGGER documentation | Never use the probe as a target power source; verify VTref before signals |

## Design boundary

The C++ adapters expose discovery, capabilities, lifecycle, telemetry, failure, and recovery as explicit simulated state. They do not open USB/serial ports, enumerate hardware, drive GPIO, program an FPGA, or issue JTAG/SWD transactions. JTAG is a debug/configuration interface with signal ownership and target-voltage requirements; it is not a generic interchangeable compute connection.

References: [Arduino Nano RP2040 Connect](https://docs.arduino.cc/hardware/nano-rp2040-connect), [Arduino pinout](https://docs.arduino.cc/resources/pinouts/ABX00053-full-pinout.pdf), [Basys 3 Reference Manual](https://digilent.com/reference/_media/reference/programmable-logic/basys-3/basys3_rm.pdf), and [SEGGER J-Link EDU Mini](https://www.segger.com/products/debug-probes/j-link/models/j-link-edu-mini/).
