# Data Format

## CAN Trace CSV

The replay engine expects one frame per row:

| Column | Required | Description |
| --- | --- | --- |
| `timestamp_ns` | yes | Capture timestamp in nanoseconds. |
| `frame_id` | yes | Decimal or hexadecimal CAN identifier. |
| `payload_hex` | yes | Up to 8 bytes encoded as hex, with or without spaces. |
| `sequence` | no | Monotonic frame counter used to detect dropped frames per CAN identifier. |

Example:

```csv
timestamp_ns,frame_id,payload_hex,sequence
0,0x123,DC1ED00700000100,1
10000000,0x124,DC05000000000100,1
```

## Replay JSON Lines

Each decoded signal is written as one JSON object:

```json
{"timestamp_ns":0,"frame_id":291,"message":"STEERING","signal":"steering_angle_deg","value":10,"unit":"deg","sequence":1}
```

This representation keeps timestamp ordering visible while making the diagnostics and metrics exporter straightforward to run on large traces.
