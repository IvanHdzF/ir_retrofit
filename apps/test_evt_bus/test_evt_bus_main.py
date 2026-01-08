import re
import typing as t
from pathlib import Path

from pytest_embedded_idf.dut import IdfDut


# ---- Regex helpers -----------------------------------------------------------

TS_PREFIX_B = rb'(?:\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\s*)?'

RE_DASHES = re.compile(rb'^' + TS_PREFIX_B + rb'-+\s*$', re.MULTILINE)
RE_SUMMARY = re.compile(
    rb'^' + TS_PREFIX_B + rb'(\d+)\s+Tests\s+(\d+)\s+Failures\s+(\d+)\s+Ignored\s*$',
    re.MULTILINE,
)
RE_RESULT_ANYWHERE = re.compile(TS_PREFIX_B + rb'(?P<result>OK|FAIL)\b')

RE_LINE_TS = re.compile(rb'(?m)^\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\s+')

ANSI_ESCAPE_RE = re.compile(
    r"""
    \x1B
    (?:
        [@-Z\\-_]
      |
        \[
        [0-?]*
        [ -/]*
        [@-~]
    )
    """,
    re.VERBOSE,
)


def strip_timestamps(raw: bytes) -> bytes:
    """Remove leading 'YYYY-MM-DD HH:MM:SS ' from each line (if present)."""
    return RE_LINE_TS.sub(b"", raw)


def strip_ansi(s: t.Union[str, bytes]) -> str:
    """Remove ANSI color/control sequences; always returns str."""
    if isinstance(s, bytes):
        s = s.decode("utf-8", errors="ignore")
    return ANSI_ESCAPE_RE.sub("", s)


def read_log_bytes(path: t.Union[str, Path]) -> bytes:
    return Path(path).read_bytes()


# ---- Test --------------------------------------------------------------------

def test_app(dut: IdfDut) -> None:
    # Sync on the Unity output (timestamps optional)
    dut.expect(RE_DASHES, timeout=60)
    dut.expect(RE_SUMMARY, timeout=1)
    dut.expect(RE_RESULT_ANYWHERE, timeout=1)

    # Load full pytest-embedded captured log, normalize it for Unity parsing
    raw = read_log_bytes(dut.logfile)
    normalized = strip_ansi(strip_timestamps(raw))

    # Add Unity testcases into the junit report
    dut.testsuite.add_unity_test_cases(normalized)

    # Assert final outcome captured by the last expect()
    assert dut.pexpect_proc.match.group("result") == b"OK"
