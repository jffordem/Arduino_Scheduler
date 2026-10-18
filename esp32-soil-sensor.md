# ESP32 Lawn Soil Moisture Sensor Network

## Concept

A network of solar-powered ESP32 nodes, each with a capacitive soil moisture sensor, deployed around the yard to build a **spatial coverage map** identifying over- and under-watered zones. Goal: diagnose sprinkler coverage gaps without manual measurement.

## Sensor Technology

**Use capacitive sensors, not resistive ones.**

- Resistive (bare metal probes): ~$1 each, corrode via electrolysis in weeks outdoors — not viable for permanent deployment.
- Capacitive (measures soil dielectric constant, no exposed metal in soil): ~$2–4 each, last years. The common blue Chinese capacitive modules or the Chirp! sensor are the standard choices.

## Node Architecture

| Component | Choice | Cost |
|-----------|--------|------|
| MCU | ESP32-C3 SuperMini | ~$2–3 |
| Sensor | Capacitive soil moisture module | ~$2–4 |
| Power | LiPo 500mAh + 1W solar panel | ~$6–9 |
| Enclosure | IP65 project box + cable gland | ~$3–6 |
| **Per node total** | | **~$13–22** |

6–8 nodes for a typical yard: **$100–175 total**.

## Firmware Sketch

- ESP32 deep sleep between readings (every 15 min is enough for irrigation analysis)
- Wake, read ADC, broadcast via **ESP-NOW** to a central collector node
- ESP32 ADC non-linearity: consider reading multiple samples and averaging; or use an external ADC (ADS1115) for better accuracy
- One collector node bridges ESP-NOW → WiFi → MQTT or HTTP

## Durability

- Sensor tip in soil: fine with capacitive boards; conformal-coat the PCB side
- Electronics enclosure: IP65 ABS or polycarbonate; UV-resistant versions matter outdoors
- Solar + LiPo handles cloudy stretches easily at 15-min polling intervals

## The Unsolved Part: Spatial Mapping

The sensor hardware is commodity. The interesting gap is **coverage visualization**:

1. Associate each node with a yard coordinate (manual setup, not GPS-level precision)
2. Collect timestamped readings from all nodes at the collector
3. Generate a **heatmap** of soil moisture overlaid on a yard sketch
4. Correlate wet/dry patches with known sprinkler zones

This layer — spatial visualization tied to your actual yard layout — is where original work lives.

## Existing Projects Worth Studying

### Directly Relevant
- **[Hackaday.io: Soil Moisture Monitoring in Flower Garden](https://hackaday.io/project/178004-soil-moisture-monitoring-in-a-flower-garden)**
  — Grid of sensors on 1m spacing, solar ESP32, MQTT to Raspberry Pi/Django, animated heatmap output. This is the closest match to what we're describing. Worth reading in full.

- **[Wireless Sensor Network for Irrigation (Qgmad)](https://github.com/Qgmad/Wireless_sensor_net_irrigation)**
  — Multi-node, sector-by-sector moisture control with web/mobile dashboard.

- **[maakbaas/esp32-soil-moisture-sensor](https://github.com/maakbaas/esp32-soil-moisture-sensor)**
  — Software + Gerber files for a clean self-contained node; good reference for PCB design.

- **[har-in-air/ESP32_AUTO_WATER](https://github.com/har-in-air/ESP32_AUTO_WATER)**
  — ESP32-C3, capacitive sensor, Google Sheets logging, OTA via AP mode. Good firmware reference.

### Platform Options
- **ESPHome + Home Assistant**: native soil moisture support, fast path to a dashboard, skips custom firmware entirely. Trade-off: locked into HA ecosystem.
- **InfluxDB + Grafana**: time-series store + visualization; easy to get a per-node moisture graph, harder to get a spatial heatmap.

## Open Questions

- Calibration: raw capacitive readings vary by soil type; dry/wet calibration per node needed
- Depth: surface reading vs. root zone (6–8" deep) — may need longer sensor stems
- Node count: how many to usefully characterize sprinkler zones?
- Contribution angle: could the spatial heatmap layer be contributed back to an existing project (Hackaday, ESPHome custom component)?
