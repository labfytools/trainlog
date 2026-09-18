#!/usr/bin/env python3
"""Exercise Web Sessions through embedded production assets in real Firefox."""

import os
import signal
import socket
import sqlite3
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / os.environ.get("TRAINLOG_WEB_E2E_BUILD", "build") / "tui"
GECKO = Path(
    os.environ.get(
        "TRAINLOG_GECKODRIVER", "/home/fy59/.cache/trainlog/web-e2e-tools/geckodriver"
    )
)
EVIDENCE = ROOT / "docs" / "reviews" / "evidence"


class BrowserSessionsTest(unittest.TestCase):
    def test_create_reload_detail_and_mobile_editor(self):
        if not GECKO.is_file():
            self.skipTest("pinned geckodriver unavailable")

        from selenium import webdriver
        from selenium.webdriver.common.by import By
        from selenium.webdriver.firefox.service import Service
        from selenium.webdriver.support.ui import WebDriverWait

        with tempfile.TemporaryDirectory(prefix="trainlog-web-sessions-") as directory:
            root = Path(directory)
            xdg = root / "data"
            xdg.mkdir()
            with socket.socket() as reserved:
                reserved.bind(("127.0.0.1", 0))
                port = reserved.getsockname()[1]
            environment = os.environ.copy()
            environment.update(
                {
                    "XDG_DATA_HOME": str(xdg),
                    "XDG_CONFIG_HOME": str(root / "config"),
                    "XDG_CACHE_HOME": str(root / "cache"),
                }
            )
            server = subprocess.Popen(
                [str(BUILD / "trainlog"), "--web", "--port", str(port)],
                env=environment,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
            driver = None
            try:
                for _ in range(200):
                    with socket.socket() as probe:
                        if probe.connect_ex(("127.0.0.1", port)) == 0:
                            break
                    time.sleep(0.025)

                options = webdriver.FirefoxOptions()
                options.add_argument("-headless")
                driver = webdriver.Firefox(options=options, service=Service(str(GECKO)))
                wait = WebDriverWait(driver, 15)
                driver.set_window_size(1440, 1000)
                driver.get(f"http://127.0.0.1:{port}/seances")
                wait.until(lambda current: "Aucun résultat" in current.page_source)
                EVIDENCE.mkdir(parents=True, exist_ok=True)
                driver.save_screenshot(str(EVIDENCE / "web-sessions-list-desktop.png"))

                driver.find_element(By.XPATH, "//button[normalize-space()='Nouvelle séance']").click()
                wait.until(lambda current: "Nouvelle séance" in current.page_source)
                title = driver.find_element(By.XPATH, "//label[normalize-space()='Titre']/input")
                title.send_keys("Séance navigateur éphémère")
                driver.set_window_size(390, 844)
                driver.save_screenshot(str(EVIDENCE / "web-sessions-editor-mobile.png"))
                save = driver.find_element(By.XPATH, "//button[normalize-space()='Enregistrer']")
                driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", save)
                save.click()
                wait.until(lambda current: "PRÉPARATION MANUELLE" in current.page_source)
                driver.refresh()
                wait.until(lambda current: "Séance navigateur éphémère" in current.page_source)
                driver.set_window_size(1440, 1000)
                driver.save_screenshot(str(EVIDENCE / "web-sessions-detail-desktop.png"))

                database_path = xdg / "trainlog" / "trainlog.db"
                with sqlite3.connect(database_path) as database:
                    row = database.execute(
                        "SELECT r.title, p.editing_state "
                        "FROM session_preparations p "
                        "JOIN session_preparation_revisions r "
                        "ON r.revision_id = p.current_revision_id"
                    ).fetchone()
                    entry_count = database.execute(
                        "SELECT COUNT(*) FROM session_preparation_entries"
                    ).fetchone()[0]
                self.assertEqual(row, ("Séance navigateur éphémère", "draft"))
                self.assertEqual(entry_count, 0)
            finally:
                if driver is not None:
                    driver.quit()
                os.killpg(server.pid, signal.SIGTERM)
                server.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
