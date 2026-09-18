#!/usr/bin/env python3
"""Exercise Web Sessions through embedded production assets in real Firefox."""

import json
import os
import signal
import socket
import sqlite3
import subprocess
import sys
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
    def test_programs_v1_import_detail_and_discrete_delete_in_real_firefox(self):
        if not GECKO.is_file():
            self.skipTest("pinned geckodriver unavailable")

        from selenium import webdriver
        from selenium.webdriver.common.by import By
        from selenium.webdriver.common.keys import Keys
        from selenium.webdriver.firefox.service import Service
        from selenium.webdriver.support.ui import WebDriverWait
        import websocket

        with tempfile.TemporaryDirectory(prefix="trainlog-web-programs-") as directory:
            root = Path(directory)
            xdg = root / "data"
            config = root / "config"
            xdg.mkdir()
            config.mkdir()
            with socket.socket() as reserved:
                reserved.bind(("127.0.0.1", 0))
                port = reserved.getsockname()[1]
            environment = os.environ.copy()
            environment.update(
                {
                    "XDG_DATA_HOME": str(xdg),
                    "XDG_CONFIG_HOME": str(config),
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
            bidi = None
            try:
                for _ in range(200):
                    with socket.socket() as probe:
                        if probe.connect_ex(("127.0.0.1", port)) == 0:
                            break
                    time.sleep(0.025)

                options = webdriver.FirefoxOptions()
                options.add_argument("-headless")
                options.set_capability("webSocketUrl", True)
                driver = webdriver.Firefox(options=options, service=Service(str(GECKO)))
                wait = WebDriverWait(driver, 15)
                bidi = websocket.create_connection(
                    driver.capabilities["webSocketUrl"], suppress_origin=True
                )
                command_id = 0

                def bidi_command(method: str, parameters: dict):
                    nonlocal command_id
                    command_id += 1
                    bidi.send(json.dumps({
                        "id": command_id,
                        "method": method,
                        "params": parameters,
                    }))
                    while True:
                        response = json.loads(bidi.recv())
                        if response.get("id") != command_id:
                            continue
                        if response.get("type") != "success":
                            raise AssertionError(response)
                        return response["result"]

                context = bidi_command("browsingContext.getTree", {})["contexts"][0]["context"]

                def set_viewport(width: int, height: int):
                    bidi_command(
                        "browsingContext.setViewport",
                        {
                            "context": context,
                            "viewport": {"width": width, "height": height},
                            "devicePixelRatio": 1,
                        },
                    )
                    wait.until(
                        lambda current: current.execute_script(
                            "return innerWidth===arguments[0]&&innerHeight===arguments[1]",
                            width,
                            height,
                        )
                    )

                set_viewport(1440, 1000)
                driver.get(f"http://127.0.0.1:{port}/seances")
                wait.until(lambda current: "Aucun résultat" in current.page_source)
                catalog = driver.execute_async_script(
                    "const done=arguments[0];"
                    "fetch('/api/v1/sessions/catalog?offset=0&limit=1')"
                    ".then((response)=>response.json()).then(done).catch(String);"
                )
                self.assertIsInstance(catalog, dict)
                self.assertTrue(catalog["items"])
                exercise = catalog["items"][0]
                occurrence = {
                    "entry_id": "pge_cccccccc-cccc-4ccc-8ccc-cccccccccccc",
                    "exercise_id": exercise["exercise_id"],
                    "equipment_id": None,
                    "load_mode": "none",
                    "rest_seconds": 90,
                    "target_sets": 3 if exercise["recording_mode"] == "sets" else None,
                    "target_reps": 8 if exercise["tracking_mode"] == "reps" else None,
                    "target_duration_seconds": (
                        None if exercise["tracking_mode"] == "reps" else 600
                    ),
                    "target_weight_kg": None,
                    "notes": None,
                }
                program = {
                    "format": "trainlog-program",
                    "version": 1,
                    "program": {
                        "program_id": "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                        "title": "Cycle navigateur V1",
                        "note": "Programme synthétique lisible.",
                        "state": "active",
                        "start_date": "2026-09-21",
                        "end_date": None,
                        "sessions": [
                            {
                                "program_session_id": "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                "title": "Séance A ordonnée",
                                "session_type": "training",
                                "planned_for": "2026-09-21",
                                "note": None,
                                "occurrences": [occurrence],
                            },
                            {
                                "program_session_id": "pgs_dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                                "title": "Séance B ordonnée",
                                "session_type": "max_test",
                                "planned_for": "2026-09-23",
                                "note": "Contrôle.",
                                "occurrences": [{**occurrence,
                                                "entry_id": "pge_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee"}],
                            },
                        ],
                    },
                }
                import_path = root / "programme-navigateur-v1.json"
                import_path.write_text(json.dumps(program), encoding="utf-8")

                driver.find_element(By.XPATH, "//button[normalize-space()='Programmes']").click()
                wait.until(lambda current: "Importer un programme" in current.page_source)
                driver.find_element(By.XPATH, "//button[normalize-space()='Importer un programme']").click()
                driver.find_element(By.CSS_SELECTOR, "input[type='file']").send_keys(str(import_path))
                wait.until(lambda current: "Aperçu avant import" in current.page_source)
                self.assertIn("2 séances", driver.page_source)
                driver.find_element(By.XPATH, "//button[normalize-space()='Importer']").click()
                wait.until(lambda current: "Cycle navigateur V1" in current.page_source)
                wait.until(lambda current: "Séance A ordonnée" in current.page_source)
                detail = driver.find_element(By.CSS_SELECTOR, "main").text
                self.assertIn("PROGRAMME · ACTIF", detail)
                self.assertIn("Import trainlog-program v1", detail)
                self.assertIn("À faire", detail)
                self.assertLess(detail.index("1. Séance A ordonnée"), detail.index("2. Séance B ordonnée"))

                driver.find_element(
                    By.XPATH, "//button[normalize-space()='← Retour aux programmes']"
                ).click()
                wait.until(lambda current: "Programme actif" in current.page_source)
                card = driver.find_element(By.CSS_SELECTOR, ".program-row").text
                self.assertIn("Cycle navigateur V1", card)
                self.assertIn("2 séances · jamais utilisé", card)
                self.assertIn("trainlog-program · importé", card)
                EVIDENCE.mkdir(parents=True, exist_ok=True)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-programs-v1-card-desktop.png")
                ))

                trash = driver.find_element(
                    By.CSS_SELECTOR, "button[aria-label='Supprimer le programme Cycle navigateur V1']"
                )
                trash.click()
                confirmation = wait.until(
                    lambda current: current.find_element(By.CSS_SELECTOR, "[role='alertdialog']")
                )
                self.assertIn("Supprimer le programme « Cycle navigateur V1 » ?", confirmation.text)
                self.assertIn("séances sources", confirmation.text)
                self.assertNotIn("Séance A ordonnée", confirmation.text)
                set_viewport(390, 844)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-programs-v1-delete-dialog-mobile-390x844.png")
                ))
                confirmation.find_element(By.XPATH, ".//button[normalize-space()='Conserver']").click()
                wait.until(lambda current: not current.find_elements(By.CSS_SELECTOR, "[role='alertdialog']"))
                self.assertEqual(driver.switch_to.active_element.get_attribute("aria-label"), trash.get_attribute("aria-label"))

                trash = driver.find_element(
                    By.CSS_SELECTOR, "button[aria-label='Supprimer le programme Cycle navigateur V1']"
                )
                trash.click()
                wait.until(lambda current: current.find_element(By.CSS_SELECTOR, "[role='alertdialog']"))
                driver.switch_to.active_element.send_keys(Keys.ESCAPE)
                wait.until(lambda current: not current.find_elements(By.CSS_SELECTOR, "[role='alertdialog']"))
                database_path = xdg / "trainlog" / "trainlog.db"
                with sqlite3.connect(database_path) as database:
                    self.assertIsNone(database.execute(
                        "SELECT deleted_at FROM programs WHERE program_id=?",
                        (program["program"]["program_id"],),
                    ).fetchone()[0])

                driver.find_element(
                    By.CSS_SELECTOR, "button[aria-label='Supprimer le programme Cycle navigateur V1']"
                ).click()
                confirmation = wait.until(
                    lambda current: current.find_element(By.CSS_SELECTOR, "[role='alertdialog']")
                )
                confirmation.find_element(
                    By.XPATH, ".//button[normalize-space()='Supprimer le programme']"
                ).click()
                wait.until(
                    lambda current: "Aucun programme." in current.page_source
                    or current.find_elements(By.CSS_SELECTOR, "[role='alert']")
                )
                self.assertNotIn("Cycle navigateur V1", driver.find_element(By.CSS_SELECTOR, "main").text)
                driver.refresh()
                wait.until(lambda current: "Cycle navigateur V1" not in current.page_source)
                with sqlite3.connect(database_path) as database:
                    deleted_at = database.execute(
                        "SELECT deleted_at FROM programs WHERE program_id=?",
                        (program["program"]["program_id"],),
                    ).fetchone()[0]
                    source_sessions = database.execute(
                        "SELECT COUNT(*) FROM program_sessions WHERE program_id=?",
                        (program["program"]["program_id"],),
                    ).fetchone()[0]
                    source_entries = database.execute(
                        "SELECT COUNT(*) FROM program_session_entries"
                    ).fetchone()[0]
                self.assertIsNotNone(deleted_at)
                self.assertEqual(source_sessions, 2)
                self.assertEqual(source_entries, 2)
            finally:
                if bidi is not None:
                    bidi.close()
                if driver is not None:
                    driver.quit()
                os.killpg(server.pid, signal.SIGTERM)
                server.wait(timeout=10)

    def test_create_preferences_reload_and_withdraw_by_real_click(self):
        if not GECKO.is_file():
            self.skipTest("pinned geckodriver unavailable")

        from selenium import webdriver
        from selenium.webdriver.common.by import By
        from selenium.webdriver.firefox.service import Service
        from selenium.webdriver.support.ui import WebDriverWait
        import websocket

        with tempfile.TemporaryDirectory(prefix="trainlog-web-sessions-") as directory:
            root = Path(directory)
            xdg = root / "data"
            config = root / "config"
            xdg.mkdir()
            config.mkdir()
            with socket.socket() as reserved:
                reserved.bind(("127.0.0.1", 0))
                port = reserved.getsockname()[1]
            environment = os.environ.copy()
            environment.update(
                {
                    "XDG_DATA_HOME": str(xdg),
                    "XDG_CONFIG_HOME": str(config),
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
            bidi = None
            try:
                for _ in range(200):
                    with socket.socket() as probe:
                        if probe.connect_ex(("127.0.0.1", port)) == 0:
                            break
                    time.sleep(0.025)

                options = webdriver.FirefoxOptions()
                options.add_argument("-headless")
                options.set_capability("webSocketUrl", True)
                driver = webdriver.Firefox(options=options, service=Service(str(GECKO)))
                wait = WebDriverWait(driver, 15)
                bidi = websocket.create_connection(
                    driver.capabilities["webSocketUrl"], suppress_origin=True
                )
                command_id = 0

                def bidi_command(method: str, parameters: dict):
                    nonlocal command_id
                    command_id += 1
                    bidi.send(json.dumps({
                        "id": command_id,
                        "method": method,
                        "params": parameters,
                    }))
                    while True:
                        response = json.loads(bidi.recv())
                        if response.get("id") != command_id:
                            continue
                        if response.get("type") != "success":
                            raise AssertionError(response)
                        return response["result"]

                contexts = bidi_command("browsingContext.getTree", {})["contexts"]
                self.assertEqual(len(contexts), 1)
                context = contexts[0]["context"]

                def set_viewport(width: int, height: int):
                    bidi_command(
                        "browsingContext.setViewport",
                        {
                            "context": context,
                            "viewport": {"width": width, "height": height},
                            "devicePixelRatio": 1,
                        },
                    )
                    wait.until(
                        lambda current: current.execute_script(
                            "return innerWidth===arguments[0]&&innerHeight===arguments[1]",
                            width,
                            height,
                        )
                    )

                set_viewport(1440, 1000)
                driver.get(f"http://127.0.0.1:{port}/seances")
                wait.until(lambda current: "Aucun résultat" in current.page_source)
                EVIDENCE.mkdir(parents=True, exist_ok=True)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-sessions-list-desktop.png")
                ))

                driver.find_element(By.XPATH, "//button[normalize-space()='Programmes']").click()
                wait.until(lambda current: "Importer un programme" in current.page_source)
                set_viewport(390, 844)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-programs-empty-mobile.png")
                ))
                driver.find_element(By.XPATH, "//button[normalize-space()='Préparation']").click()
                wait.until(lambda current: "Nouvelle séance" in current.page_source)

                driver.find_element(By.XPATH, "//button[normalize-space()='Nouvelle séance']").click()
                wait.until(lambda current: "Nouvelle séance" in current.page_source)
                title = driver.find_element(By.XPATH, "//label[normalize-space()='Titre']/input")
                title.send_keys("Séance navigateur éphémère")
                planned = driver.find_element(
                    By.XPATH, "//label[normalize-space()='Date planifiée']/input"
                )
                driver.execute_script(
                    "const setter=Object.getOwnPropertyDescriptor("
                    "HTMLInputElement.prototype,'value').set;"
                    "setter.call(arguments[0],'2026-09-18');"
                    "arguments[0].dispatchEvent(new Event('input',{bubbles:true}));"
                    "arguments[0].dispatchEvent(new Event('change',{bubbles:true}));",
                    planned,
                )
                catalog_choice = wait.until(
                    lambda current: current.find_element(
                        By.CSS_SELECTOR, ".catalog-results button"
                    )
                )
                catalog_choice.click()
                set_viewport(390, 844)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-sessions-editor-mobile.png")
                ))
                save = driver.find_element(
                    By.XPATH,
                    "//button[normalize-space()='Marquer prête / Préparer pour Android']",
                )
                driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", save)
                save.click()
                wait.until(lambda current: "PRÉPARATION MANUELLE" in current.page_source)
                driver.refresh()
                wait.until(lambda current: "Séance navigateur éphémère" in current.page_source)
                set_viewport(1440, 1000)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-sessions-detail-desktop.png")
                ))

                self.assertIn("18/09/2026", driver.page_source)
                driver.find_element(
                    By.XPATH, "//button[normalize-space()='Paramètres d’affichage']"
                ).click()
                wait.until(lambda current: "Format des dates" in current.page_source)
                driver.find_element(By.CSS_SELECTOR, "input[value='iso']").click()
                wait.until(
                    lambda current: current.find_element(
                        By.CSS_SELECTOR, "main time"
                    ).text == "2026-09-18"
                )
                driver.refresh()
                wait.until(
                    lambda current: current.find_element(
                        By.CSS_SELECTOR, "main time"
                    ).text == "2026-09-18"
                )
                preferences = (
                    config / "trainlog" / "web" / "preferences-v1.json"
                ).read_text(encoding="utf-8")
                self.assertIn('"date_format":"iso"', preferences)

                driver.find_element(
                    By.XPATH, "//button[normalize-space()='Supprimer la préparation']"
                ).click()
                confirmation = wait.until(
                    lambda current: current.find_element(By.CSS_SELECTOR, "[role='alertdialog']")
                )
                self.assertIn("Séance navigateur éphémère", confirmation.text)
                set_viewport(390, 844)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-sessions-delete-confirmation-mobile.png")
                ))
                confirmation.find_element(
                    By.XPATH, ".//button[normalize-space()='Supprimer la préparation']"
                ).click()
                wait.until(
                    lambda current: "annulation Android à synchroniser" in current.page_source
                )
                set_viewport(1440, 1000)
                self.assertTrue(driver.save_screenshot(
                    str(EVIDENCE / "web-sessions-withdrawn-detail-desktop.png")
                ))
                driver.refresh()
                wait.until(lambda current: "Supprimée" in current.page_source)
                self.assertNotIn(
                    "Supprimer la préparation",
                    driver.find_element(By.CSS_SELECTOR, "main").text,
                )

                database_path = xdg / "trainlog" / "trainlog.db"
                with sqlite3.connect(database_path) as database:
                    row = database.execute(
                        "SELECT r.title, p.editing_state, p.withdrawn_at "
                        "FROM session_preparations p "
                        "JOIN session_preparation_revisions r "
                        "ON r.revision_id = p.current_revision_id"
                    ).fetchone()
                    entry_count = database.execute(
                        "SELECT COUNT(*) FROM session_preparation_entries"
                    ).fetchone()[0]
                    withdrawal_count = database.execute(
                        "SELECT COUNT(*) FROM session_preparation_withdrawals"
                    ).fetchone()[0]
                    delivery_count = database.execute(
                        "SELECT COUNT(*) FROM session_preparation_deliveries"
                    ).fetchone()[0]
                self.assertEqual(row[:2], ("Séance navigateur éphémère", "ready"))
                self.assertIsNotNone(row[2])
                self.assertEqual(entry_count, 1)
                self.assertEqual(delivery_count, 1)
                self.assertEqual(withdrawal_count, 1)
                export_path = root / "session-preparations-v2.json"
                subprocess.run(
                    [
                        sys.executable,
                        str(ROOT / "tools" / "export_session_preparations.py"),
                        str(export_path),
                        "--database",
                        str(database_path),
                    ],
                    check=True,
                    capture_output=True,
                    text=True,
                )
                exported = json.loads(export_path.read_text(encoding="utf-8"))
                self.assertEqual(exported["deliveries"], [])
                self.assertEqual(len(exported["withdrawals"]), 1)
                self.assertEqual(len(exported["withdrawals"][0]["deliveries"]), 1)
            finally:
                if bidi is not None:
                    bidi.close()
                if driver is not None:
                    driver.quit()
                os.killpg(server.pid, signal.SIGTERM)
                server.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
