@'
# VANET Emergency Dissemination

Design and experimental evaluation of message dissemination protocols
for fragmented Vehicular Ad Hoc Networks.

This repository accompanies the Master of Engineering research project:

**Emergency Dissemination in Vehicular Ad Hoc Networks: Design and
Evaluation of a Bounded Gossip-Based Protocol**

## Protocols

The project evaluates three Contiki-NG protocol implementations:

- **BGKP**, Bounded Gossip-Based Kernel Protocol
- **FLUD**, Flooding UDP baseline
- **DUMB**, Direct Unicast Minimal Beaconing baseline

BGKP combines bounded retransmission, preferred-peer selection,
store-carry-forward buffering, and state-based emergency dissemination.

FLUD and DUMB are independent baseline implementations used for
comparative evaluation. They are not versions of BGKP.

## Repository contents

- `FW/Cooja/BGKP` contains BGKP firmware for Cooja Sky motes.
- `FW/Cooja/FLUD` contains the flooding baseline firmware.
- `FW/Cooja/DUMB` contains the minimal dissemination baseline firmware.
- `FW/CC1352R-Launchpad/BGKP` contains the preliminary BGKP hardware port.
- `Map` contains OpenStreetMap, SUMO, mobility, and Cooja scenario assets.
- `Scripts` contains scenario-generation and post-processing tools.
- `Logs` contains selected simulation logs, summaries, and plots.
- `Documents` contains the report, poster, and system documentation.

## Node roles

Each implementation includes two logical node roles:

- **Mobile nodes** represent vehicles and generate or forward messages.
- **Roadside Units** act as intermittent infrastructure access points
  and data sinks.

## Simulation environments

### Urban

The urban scenario is based on OpenStreetMap data for Wynyard Quarter.
SUMO generates vehicle movement, which is converted into mobility traces
for Cooja.

The final comparative analysis uses six fixed RSUs and mobile-node
densities from 10 to 90 vehicles.

### Highway

The highway scenario uses a scripted bidirectional mobility model inside
Cooja. Six RSUs are distributed across a simplified 5 km environment,
with mobile-node densities from 10 to 100 vehicles.

## Hardware implementation

A preliminary BGKP port was tested on TI SimpleLink CC1352R LaunchPad
boards running Contiki-NG.

The hardware validation covered:

- firmware execution;
- IEEE 802.15.4 communication;
- QUERY and DATA exchange;
- acknowledgement handling;
- UART-based protocol observation.

The hardware validation did not include GNSS input or a quantitative
performance evaluation.

## Post-processing

Simulation firmware emits structured `METR` records.

The processing workflow uses:

- `Scripts/PostProcessing/metrics_parser.py` to extract delivery,
  latency, transmission, buffer, and emergency metrics;
- `Scripts/PostProcessing/GraphsFromBatch.py` to produce combined
  summaries, CSV files, and comparative plots;
- `Scripts/PostProcessing/getMetrics_v01.bat` to run the Windows
  batch-processing workflow.

## Results summary

The evaluation shows scenario-dependent trade-offs rather than a
universal advantage for BGKP.

- BGKP achieves the highest RSU-delivered data ratio across the evaluated
  urban densities.
- The urban delivery benefit is accompanied by substantially higher
  latency and transmission overhead.
- BGKP's highway delivery advantage is limited to the lowest densities.
- FLUD generally provides faster emergency convergence in the highway
  scenario.

BGKP is therefore presented as a bounded, fragmentation-aware trade-off
design rather than a dominant dissemination solution.

## Platforms and tools

- Contiki-NG
- Cooja
- Sky motes
- TI SimpleLink CC1352R LaunchPad
- IEEE 802.15.4
- IPv6/6LoWPAN
- UDP
- SUMO
- OpenStreetMap
- Python

## Documentation

The `Documents` directory contains:

- the editable DOCX source and published PDF of the final report;
- the editable DOCX source and published PDF of the project poster;
- the current BGKP system-design document.

## Limitations

This repository contains a research prototype. It is not a
production-ready or safety-certified V2X implementation.

The completed evaluation uses one run per
scenario/protocol/density combination and does not quantify run-to-run
variance.

## Author

Boris Gor  
Master of Engineering  
Auckland University of Technology
