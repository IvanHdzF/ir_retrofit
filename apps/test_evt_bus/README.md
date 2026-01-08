# evt_bus Test Application (HIL)

This application contains **hardware-in-the-loop (HIL) integration tests** for the `evt_bus` component, built with **ESP-IDF** and executed on a **real ESP board** via `pytest-embedded`.

- Supported targets: **esp32**, **esp32s3**
- Unity test implementation: `test_evt_bus_main.c`
- Host-side test harness: `apps/test_evt_bus/test_evt_bus_main.py`

---

## What’s being tested

The firmware runs **Unity** tests on-device and prints standard Unity output.  
The Python harness connects to the DUT, synchronizes on Unity output, and then parses the captured log to:

- Verify overall PASS/FAIL (`OK` / `FAIL`)
- Strip optional timestamp prefixes (e.g. `YYYY-MM-DD HH:MM:SS `)
- Strip ANSI escape sequences (color/control codes)
- Inject Unity testcases into the generated JUnit report

---

## Run (HIL)

From the **repo root**:

```bash
pytest   --junit-xml=./test_app_results_esp32s3.xml   --embedded-services idf,esp   --target=esp32s3
```

### Notes
- `--embedded-services idf,esp` selects the ESP-IDF build + **real hardware** runner (serial DUT).
- `--target` selects the ESP-IDF target (`esp32` or `esp32s3`).

---

## Files

```
test_evt_bus/
  ├─ CMakeLists.txt           # ESP-IDF project definition
  ├─ pytest.ini               # pytest configuration for embedded tests
  ├─ requirements.txt         # Python dependencies for the test harness
  ├─ test_evt_bus_main.c      # Unity tests running on the device (DUT)
  ├─ test_evt_bus_main.py     # pytest HIL harness (expects, log parsing, JUnit injection)
  └─ README.md
```

---

## Adding tests

1. Add new Unity tests to `test_evt_bus_main.c`
2. Ensure output matches Unity conventions (dashes + summary line)
3. Run the pytest command above and confirm JUnit output includes testcases

---

## Troubleshooting

### Unity output has timestamps / colors
The harness already normalizes logs by:
- removing leading timestamps per-line (if present)
- removing ANSI escape sequences

### Timeouts waiting for Unity output
Increase timeouts in `test_evt_bus_main.py` or confirm:
- correct serial port / board is connected
- the firmware flashed successfully
- the DUT boots and prints Unity banner/summary
