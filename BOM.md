# Inside Voice — Bill of Materials

## Core Electronics

| Ref | Component | Specs | Qty | Est. Price | Source |
|-----|-----------|-------|-----|------------|--------|
| U1 | Seeed Studio XIAO nRF52840 Sense | nRF52840, BLE 5.0, PDM mic, RGB LED, USB-C, BQ25101 LiPo charger | 1 | $15.99 | [Seeed Studio](https://www.seeedstudio.com/Seeed-XIAO-BLE-Sense-nRF52840-p-5253.html) |
| BT1 | 3.7V LiPo Battery (302530) | 200–250 mAh, 30 × 25 × 3 mm, JST or bare leads with protection circuit | 1 | $3–5 | AliExpress (search "302530 lipo 3.7v") |
| M1 | NFP-C1034 Coin Vibration Motor | 10 mm × 3.4 mm, 3.0V rated, ~85 mA, 1.5G amplitude | 1 | $2–5 | [NFP Shop](https://nfpshop.com/product/10mm-coin-vibration-motor-3-4mm-type-model-nfp-c1034-coin-vibration-motor) |
| Q1 | IRLML6344 N-Channel MOSFET | SOT-23, Vgs(th) 0.8V, Rds(on) 29 mΩ, logic-level | 1 | $0.50 | DigiKey / LCSC |

## Passive Components

| Ref | Component | Specs | Qty | Est. Price | Notes |
|-----|-----------|-------|-----|------------|-------|
| D1 | 1N4148 or equivalent | Flyback diode across motor | 1 | $0.05 | Protects Q1 from back-EMF |
| R1 | 10 kΩ resistor | 0402/0603, pull-down on Q1 gate | 1 | $0.01 | Keeps motor off when GPIO floating |

## Mechanical / Assembly

| Item | Description | Qty | Notes |
|------|-------------|-----|-------|
| Lapel pin backing | Brooch pin or magnetic clasp | 1 | Attach to enclosure |
| Enclosure | 3D-printed or molded case, ~35 × 30 × 10 mm target | 1 | Must fit XIAO + battery + motor |
| Wire | 28–30 AWG silicone | ~15 cm | Motor leads, battery leads to BAT+/BAT- pads |

## Connections Summary

```
USB-C ──→ XIAO nRF52840 Sense (U1)
              │
              ├── BAT+/BAT- pads ──→ LiPo Battery (BT1)
              │     (BQ25101 onboard charger, 50/100 mA selectable via P0.13)
              │
              ├── D0 (GPIO0 pin 2, PWM1 ch0) ──→ Q1 Gate (IRLML6344)
              │                                      │
              │                              Q1 Drain ──→ M1 (−) motor terminal
              │                              Q1 Source ──→ GND
              │
              ├── 3V3 ──→ M1 (+) motor terminal
              │              ├── D1 flyback diode (cathode to 3V3, anode to drain)
              │
              └── Onboard: PDM mic, RGB LED (no external wiring)
```

## Power Notes

- Average draw: ~8–12 mA (BLE + PDM mic + CPU, DC-DC enabled)
- Motor peak: ~85 mA (intermittent, during threshold alerts only)
- Runtime: ~25 hours with 250 mAh cell at 10 mA average
- Charging: USB-C → BQ25101 → 50 mA (default) or 100 mA (P0.13 driven LOW)
- Charge time: ~5 hours at 50 mA for 250 mAh cell

## Estimated Total Cost

~$22–27 per unit (single quantity, retail pricing)

## Alternative Components

### Battery Alternatives
| Option | Capacity | Dimensions | Runtime @10mA | Source |
|--------|----------|------------|---------------|--------|
| Adafruit #1317 | 150 mAh | 20 × 26 × 3.8 mm | ~15 hrs | adafruit.com ($5.95) |
| Adafruit #2750 | 350 mAh | 36 × 20 × 5.2 mm | ~35 hrs | adafruit.com ($6.95) |
| Adafruit #3898 | 400 mAh | 36 × 17 × 7.8 mm | ~40 hrs | adafruit.com ($6.95) |

### Motor Alternatives
| Option | Size | Vibration | Current | Notes |
|--------|------|-----------|---------|-------|
| NFP-C0830 | 8 mm × 3 mm | 0.74G (subtle) | ~65 mA | Smaller, more discreet |
| Adafruit #1201 | 10 mm × 2.7 mm | Moderate | ~60 mA | Easy to source, good for prototyping |

### MOSFET Alternatives
| Option | Package | Vgs(th) | Notes |
|--------|---------|---------|-------|
| Si2302 | SOT-23 | 0.7V | Good logic-level option |
| 2N7002 | SOT-23 | 1.0–2.5V | Marginal at 3.3V gate drive |
