from __future__ import annotations

import pytest
from fastapi.testclient import TestClient

from agent import main as main_mod


@pytest.fixture
def api_client(monkeypatch: pytest.MonkeyPatch):
    monkeypatch.setenv("MIKROTOM_SQLITE_PATH", "off")
    with TestClient(main_mod.app) as client:
        yield client
