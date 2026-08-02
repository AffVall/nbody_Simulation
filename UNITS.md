# Gravity Simulation — Unit System

## Internal Units (JSON storage)

| Quantity | Unit | Symbol |
|----------|------|--------|
| Mass | Solar mass | M☉ |
| Distance | Astronomical Unit | AU |
| Time | Year | yr |
| Velocity | AU/yr | — |

## Display Units (Builder UI)

| Quantity | star | planet | moon | comet/asteroid |
|----------|------|--------|------|----------------|
| Mass | M☉ | M⊕ | M☽ | M☉ |
| Radius | R☉ | R⊕ | R☽ | AU |
| Velocity | km/s | km/s | km/s | km/s |
| Position | AU | AU | AU | AU |

## Physical Constants

| Constant | Value | Unit |
|----------|-------|------|
| G (gravitational) | 39.478 | AU³/(M☉·yr²) |
| c (speed of light) | 63241.1 | AU/yr |
| 4π² | 39.478 | — |

## Conversion Factors

### Mass

| From | To | Factor |
|------|----|--------|
| 1 M⊕ | M☉ | 3.003×10⁻⁶ |
| 1 M☽ | M☉ | 3.691×10⁻⁸ |
| 1 M☉ | kg | 1.989×10³⁰ |

### Radius

| From | To | Factor |
|------|----|--------|
| 1 R☉ | AU | 0.00465 |
| 1 R⊕ | AU | 4.26×10⁻⁵ |
| 1 R☽ | AU | 1.737×10⁻⁵ |
| 1 AU | km | 1.496×10⁸ |

### Velocity

| From | To | Factor |
|------|----|--------|
| 1 AU/yr | km/s | 4.74047 |
| 1 km/s | AU/yr | 0.21095 |

### Distance

| From | To | Factor |
|------|----|--------|
| 1 AU | km | 1.496×10⁸ |
| 1 km | AU | 6.6846×10⁻⁹ |

## Reference Values

| Object | Mass | Radius | Orbital Speed |
|--------|------|--------|---------------|
| Sun | 1 M☉ | 1 R☉ | — |
| Earth | 1 M⊕ | 1 R⊕ | 29.78 km/s |
| Moon | 0.0123 M⊕ | 0.273 R⊕ | 1.022 km/s |
| Jupiter | 317.8 M⊕ | 11.21 R⊕ | 13.07 km/s |
| Mercury | 0.0553 M⊕ | 0.383 R⊕ | 47.36 km/s |

## JSON Preset Format

```json
{
    "name": "System Name",
    "gravity": 39.478,
    "time_step": 0.001,
    "relativistic": false,
    "relativistic_intensity": 1.0,
    "speed_of_light": 63241.1,
    "bodies": [
        {
            "name": "Star",
            "type": "star",
            "mass": 1.0,
            "radius": 0.00465,
            "position": [0.0, 0.0, 0.0],
            "velocity": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0],
            "color": [1.0, 0.9, 0.3]
        }
    ]
}
```

All values stored in internal units (M☉, AU, yr). The Builder converts to display units automatically.
